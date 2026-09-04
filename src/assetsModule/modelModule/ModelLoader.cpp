#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/GltfMaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Animation.h"
#include "MeshUtils.h"
#include "MeshVaoRegistry.h"
#include "logsModule/logger.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/ArmatureComponent.h"
#include "core/FileSystem.h"
#include "multithreading/ThreadPool.h"
#include "mathModule/VectorOperations.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <tuple>

using namespace AssetsModule;

namespace {
	bool meshHasSkin(const SFE::MeshObject3D& mesh) {
		for (const auto& v : mesh.mesh.vertices) {
			if (v.boneIDs[0] >= 0) {
				return true;
			}
		}
		return false;
	}

	void bakeRigidMesh(SFE::MeshObject3D& mesh) {
		const auto t = mesh.transform;
		if (t == SFE::Math::Mat4(1.f)) {
			return;
		}
		for (auto& vertex : mesh.mesh.vertices) {
			vertex.position = t * vertex.position;
			const auto n = t * SFE::Math::Vec4(vertex.normal, 0.f);
			const auto tan = t * SFE::Math::Vec4(vertex.tangent, 0.f);
			const auto bit = t * SFE::Math::Vec4(vertex.biTangent, 0.f);
			vertex.normal = SFE::Math::normalize(SFE::Math::Vec3(n.x, n.y, n.z));
			vertex.tangent = { tan.x, tan.y, tan.z };
			vertex.biTangent = { bit.x, bit.y, bit.z };
		}
		mesh.transform = SFE::Math::Mat4(1.f);
	}

	void bindUnskinnedMeshes(SFE::Tree<SFE::MeshObject3D>& meshes, const Armature& armature) {
		std::map<std::string, const Bone*> byName;
		for (const auto& bone : armature.bones) {
			byName[bone.name] = &bone;
		}
		const SFE::Math::Mat4 identity(1.f);

		for (auto& node : meshes) {
			auto& mesh = node.value;
			if (mesh.mesh.vertices.empty() || meshHasSkin(mesh)) {
				continue;
			}

			const bool needBake = mesh.transform != identity;
			if (needBake) {
				bakeRigidMesh(mesh);
			}

			const Bone* nearest = nullptr;
			const Bone* deforming = nullptr;
			for (auto* p = &node; p; p = p->parent) {
				const auto it = byName.find(p->value.nodeName);
				if (it == byName.end()) {
					continue;
				}
				if (!nearest) {
					nearest = it->second;
				}
				if (it->second->offset != identity && !deforming) {
					deforming = it->second;
				}
			}
			const Bone* chosen = needBake ? (deforming ? deforming : nearest) : nearest;
			if (chosen) {
				for (auto& v : mesh.mesh.vertices) {
					v.boneIDs[0] = static_cast<int>(chosen->id);
					v.weights[0] = 1.f;
				}
			}

			std::string nl = mesh.nodeName;
			std::transform(nl.begin(), nl.end(), nl.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const bool shellName = nl.find("cornea") != std::string::npos
				|| nl.find("lens") != std::string::npos
				|| nl.find("eyeball") != std::string::npos
				|| nl.find("visor") != std::string::npos;
			if (shellName && !mesh.material.alphaCutout) {
				mesh.material.transparent = true;
				mesh.material.opacity = 0.42f;
				if (mesh.material.ior < 1.05f) {
					mesh.material.ior = 1.4f;
				}
			}
		}
	}
}

SFE::Math::Mat4 ModelLoader::assimpMatToMat4(const aiMatrix4x4& from) {
	SFE::Math::Mat4 to;

	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;

	return to;
}

AssetsModule::Model* ModelLoader::load(const std::string& path) {
	auto asset = AssetsManager::instance()->getAsset<Model>(path);
	if (asset) {
		return asset;
	}

	mtx.lock();
	auto searchLock = loading.find(path);
	if (searchLock != loading.end()) {
		mtx.unlock();
		auto mutex = std::unique_lock(mtx);
		searchLock->second.wait(mutex);
		return AssetsManager::instance()->getAsset<Model>(path);
	}

	loading[path];
	mtx.unlock();

	SFE::Tree<SFE::MeshObject3D> meshes;
	Armature armatur;
	std::vector<Animation> animations;
	bool loadedOk = false;
	{
		Assimp::Importer import;
		import.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
		const unsigned flags = aiProcess_Triangulate | aiProcess_FlipUVs
			| aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights;
		const auto diskPath = SFE::FileSystem::resolve(path).string();
		const aiScene* scene = import.ReadFile(diskPath, flags);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			SFE::LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		}
		else {
			auto loaded = loadModel(scene, diskPath);
			meshes = std::move(std::get<0>(loaded));
			armatur = std::move(std::get<1>(loaded));
			animations.reserve(scene->mNumAnimations);
			for (auto i = 0u; i < scene->mNumAnimations; i++) {
				animations.emplace_back(scene->mAnimations[i]);
			}

			for (auto& meshNode : meshes) {
				auto& mesh = meshNode.value;
				if (mesh.mesh.vertices.empty()) {
					continue;
				}
				auto minAABB = SFE::Math::Vec3(std::numeric_limits<float>::max());
				auto maxAABB = SFE::Math::Vec3(std::numeric_limits<float>::lowest());
				for (const auto& vertex : mesh.mesh.vertices) {
					minAABB.x = std::min(minAABB.x, vertex.position.x);
					minAABB.y = std::min(minAABB.y, vertex.position.y);
					minAABB.z = std::min(minAABB.z, vertex.position.z);
					maxAABB.x = std::max(maxAABB.x, vertex.position.x);
					maxAABB.y = std::max(maxAABB.y, vertex.position.y);
					maxAABB.z = std::max(maxAABB.z, vertex.position.z);
				}
				mesh.aabb = SFE::FrustumModule::AABB(minAABB, maxAABB);
			}
			loadedOk = true;
		}
	}

	if (!loadedOk) {
		mtx.lock();
		loading[path].notify_all();
		loading.erase(path);
		mtx.unlock();
		return nullptr;
	}

	mtx.lock();
	asset = AssetsManager::instance()->createAsset<Model>(path, std::move(meshes), std::move(armatur), std::move(animations));
	loading[path].notify_all();
	loading.erase(path);
	mtx.unlock();

	return asset;
}

std::tuple<SFE::Tree<SFE::MeshObject3D>, Armature> ModelLoader::loadModel(const aiScene* scene, const std::string& path) {
	const auto directory = std::filesystem::path(path).parent_path().string();

	SFE::Tree<SFE::MeshObject3D> meshes;
	Armature armat;

	processNode(scene->mRootNode, scene, directory, meshes, armat);
	finalizeArmature(scene, armat);
	bindUnskinnedMeshes(meshes, armat);

	return { meshes, armat };
}


void ModelLoader::processNode(aiNode* node, const aiScene* scene, const std::string& directory, SFE::Tree<SFE::MeshObject3D>& meshes, Armature& armature) {
	meshes.value.nodeName = node->mName.C_Str();
	{
		meshes.value.transform = assimpMatToMat4(node->mTransformation);

		auto parent = meshes.parent;
		while (parent) {
			meshes.value.transform *= parent->value.transform;
			parent = parent->parent;
		}
	}

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		SFE::MeshObject3D* dst = &meshes.value;
		if (i > 0 || !meshes.value.mesh.vertices.empty()) {
			meshes.addChild({});
			meshes.children.front().value.transform = meshes.value.transform;
			meshes.children.front().value.nodeName = meshes.value.nodeName;
			dst = &meshes.children.front().value;
		}
		processMesh(scene->mMeshes[node->mMeshes[i]], scene, directory, *dst, armature);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		meshes.addChild({});
		processNode(node->mChildren[i], scene, directory, meshes.children.front(), armature);
	}
}

void ModelLoader::readBonesData(std::vector<SFE::Vertex3D>& vertices, aiMesh* mesh, Armature& armature) {
	auto& bones = armature.bones;
	bones.reserve(mesh->mNumBones);

	std::map<std::string, int> bonesIds;

	for (const auto& bone : bones) {
		bonesIds[bone.name] = bone.id;
	}

	for (auto boneIndex = 0u; boneIndex < mesh->mNumBones; ++boneIndex) {
		uint32_t boneID;
		const auto meshBone = mesh->mBones[boneIndex];
		std::string boneName = meshBone->mName.C_Str();
		auto id = bonesIds.find(boneName);
		if (id == bonesIds.end()) {
			boneID = static_cast<uint32_t>(bones.size());

			Bone bone;

			bone.offset = assimpMatToMat4(meshBone->mOffsetMatrix);
			bone.name = boneName;
			bone.id = boneID;
			
			bones.push_back(bone);

			bonesIds[boneName] = boneID;
		}
		else {
			boneID = id->second;
		}

		assert(boneID != -1);
		const auto weights = meshBone->mWeights;

		for (auto weightIndex = 0u; weightIndex < meshBone->mNumWeights; ++weightIndex) {
			const auto vertexId = weights[weightIndex].mVertexId;
			const auto weight = weights[weightIndex].mWeight;
			if (vertexId >= vertices.size()) {
				assert(vertexId < vertices.size());
				continue;
			}
			
			for (int i = 0; i < 4; ++i) {
				if (vertices[vertexId].boneIDs[i] < 0) {
					vertices[vertexId].weights[i] = weight;
					vertices[vertexId].boneIDs[i] = boneID;
					break;
				}
			}
		}
	}
}

void ModelLoader::finalizeArmature(const aiScene* scene, Armature& armature) {
	if (!scene || !scene->mRootNode || armature.bones.empty()) {
		return;
	}

	constexpr uint32_t kNone = std::numeric_limits<uint32_t>::max();
	std::map<std::string, uint32_t> bonesIds;
	for (const auto& bone : armature.bones) {
		bonesIds[bone.name] = bone.id;
	}

	auto tryAddNode = [&](const aiNode* node) {
		if (!node || armature.bones.size() >= static_cast<size_t>(SFE::ComponentsModule::kMaxArmatureBones)) {
			return;
		}
		const std::string name = node->mName.C_Str();
		if (name.empty() || bonesIds.contains(name)) {
			return;
		}
		Bone bone;
		bone.name = name;
		bone.id = static_cast<uint32_t>(armature.bones.size());
		bone.offset = SFE::Math::Mat4(1.f);
		bone.transform = assimpMatToMat4(node->mTransformation);
		armature.bones.push_back(bone);
		bonesIds[name] = bone.id;
	};

	tryAddNode(scene->mRootNode);

	const size_t deformingCount = armature.bones.size();
	for (size_t i = 0; i < deformingCount; ++i) {
		const aiNode* node = scene->mRootNode->FindNode(armature.bones[i].name.c_str());
		while (node) {
			tryAddNode(node);
			node = node->mParent;
		}
	}

	for (auto& bone : armature.bones) {
		bone.parentBoneIdx = kNone;
		bone.childrenBones.clear();
		if (const aiNode* node = scene->mRootNode->FindNode(bone.name.c_str())) {
			bone.transform = assimpMatToMat4(node->mTransformation);
			if (node->mParent) {
				const auto it = bonesIds.find(node->mParent->mName.C_Str());
				if (it != bonesIds.end()) {
					bone.parentBoneIdx = it->second;
				}
			}
		}
	}

	for (auto& bone : armature.bones) {
		if (bone.parentBoneIdx != kNone && bone.parentBoneIdx < armature.bones.size()) {
			armature.bones[bone.parentBoneIdx].childrenBones.push_back(bone.id);
		}
	}

	armature.transform = SFE::Math::Mat4(1.f);
}

void ModelLoader::readMaterialData(SFE::Material& material, aiMaterial* assimpMaterial, const std::string& directory) {
	auto bindFirst = [&material](SFE::MaterialType type, const std::vector<Texture*>& maps) {
		if (maps.empty() || material.tryGetTexture(type)) {
			return;
		}
		material[type] = SFE::MaterialTexture{ maps.front()->texture, type, type };
	};

	auto tryFiles = [&material, &directory](SFE::MaterialType type, std::initializer_list<std::string> names) {
		if (material.tryGetTexture(type)) {
			return;
		}
		for (const auto& name : names) {
			const std::string path = directory + "/" + name;
			if (!SFE::FileSystem::isFileExists(path)) {
				continue;
			}
			auto* tex = TextureHandler::loadTexture(path);
			if (tex && tex->isValid()) {
				material[type] = SFE::MaterialTexture{ tex->texture, type, type };
				return;
			}
		}
	};

	auto diffuseMaps = loadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE, directory);
	if (diffuseMaps.empty()) {
		diffuseMaps = loadMaterialTextures(assimpMaterial, aiTextureType_BASE_COLOR, directory);
	}
	bindFirst(SFE::DIFFUSE, diffuseMaps);

	auto specularMaps = loadMaterialTextures(assimpMaterial, aiTextureType_SPECULAR, directory);
	bindFirst(SFE::SPECULAR, specularMaps);

	auto roughnessMaps = loadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE_ROUGHNESS, directory);
	auto metallicMaps = loadMaterialTextures(assimpMaterial, aiTextureType_METALNESS, directory);
	auto packedMaps = loadMaterialTextures(assimpMaterial, aiTextureType_UNKNOWN, directory);
	if (roughnessMaps.empty()) {
		roughnessMaps = packedMaps;
	}
	if (metallicMaps.empty()) {
		metallicMaps = packedMaps;
	}
	bindFirst(SFE::ROUGHNESS, roughnessMaps);
	bindFirst(SFE::METALLIC, metallicMaps);

	auto normalMaps = loadMaterialTextures(assimpMaterial, aiTextureType_NORMALS, directory);
	if (normalMaps.empty()) {
		normalMaps = loadMaterialTextures(assimpMaterial, aiTextureType_NORMAL_CAMERA, directory);
	}
	bindFirst(SFE::NORMALS, normalMaps);

	aiString matName;
	assimpMaterial->Get(AI_MATKEY_NAME, matName);
	std::string n = matName.C_Str();
	std::string nUs = n;
	std::string nSp = n;
	std::replace(nUs.begin(), nUs.end(), ' ', '_');
	std::replace(nSp.begin(), nSp.end(), '_', ' ');

	tryFiles(SFE::DIFFUSE, {
		n + "_Base_Color.png", n + "_BaseColor.png", n + "_albedo.png", n + "_color.png",
		nUs + "_Base_Color.png", nUs + "_BaseColor.png", nUs + "_color.png",
		nSp + "_Base_Color.png", nSp + "_BaseColor.png"
	});
	tryFiles(SFE::NORMALS, {
		n + "_Normal.png", n + "_Normal_OpenGL.png", n + "_normal.png",
		nUs + "_Normal.png", nUs + "_normal.png", nSp + "_Normal.png"
	});
	tryFiles(SFE::ROUGHNESS, {
		n + "_Roughness.png", n + "_rough.png", nUs + "_Roughness.png", nUs + "_rough.png", nSp + "_Roughness.png"
	});
	tryFiles(SFE::METALLIC, {
		n + "_Metallic.png", n + "_metal.png", nUs + "_Metallic.png", nUs + "_metal.png", nSp + "_Metallic.png"
	});
	tryFiles(SFE::SPECULAR, {
		n + "_Specular.png", n + "_spec.png", nUs + "_Specular.png", nUs + "_spec.png", nSp + "_Specular.png"
	});
	{
		const auto us = n.rfind('_');
		const std::string atlas = (us != std::string::npos && us > 0) ? n.substr(0, us) : n;
		tryFiles(SFE::SUBSURFACE, {
			atlas + "_subsur.png", atlas + "_sss.png", atlas + "_subsurface.png",
			n + "_subsur.png", nUs + "_subsur.png", n + "_sss.png"
		});
	}

	float opacity = 1.f;
	assimpMaterial->Get(AI_MATKEY_OPACITY, opacity);
	float ior = 1.5f;
	assimpMaterial->Get(AI_MATKEY_REFRACTI, ior);
	float transmission = 0.f;
	assimpMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission);

	std::string nl = n;
	std::transform(nl.begin(), nl.end(), nl.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const bool nameGlass = nl.find("glass") != std::string::npos
		|| nl.find("window") != std::string::npos
		|| nl.find("transparent") != std::string::npos
		|| nl.find("water") != std::string::npos;
	const bool nameCutout = nl.find("fur") != std::string::npos
		|| nl.find("hair") != std::string::npos
		|| nl.find("whisker") != std::string::npos;

	aiString alphaMode;
	assimpMaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
	const std::string am = alphaMode.C_Str();

	material.opacity = opacity;
	material.ior = ior > 1.f ? ior : 1.5f;
	float shininess = 0.f;
	if (assimpMaterial->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 1.f) {
		material.roughness = (std::max)(0.04f, (std::min)(1.f, std::sqrt(2.f / (shininess + 2.f))));
	}
	float metalFactor = 0.f;
	if (assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalFactor) == AI_SUCCESS) {
		material.metallic = (std::max)(0.f, (std::min)(1.f, metalFactor));
	}
	// FBX Phong often stores TransparencyFactor=1 as "unused", not glass.
	material.transparent = nameGlass || transmission > 0.01f || (opacity > 0.05f && opacity < 0.99f)
		|| am == "BLEND";
	material.alphaCutout = !material.transparent && (am == "MASK" || nameCutout);
}

void ModelLoader::readIndicesData(std::vector<unsigned>& indices, unsigned numFaces, aiFace* faces) {
	indices.clear();
	indices.reserve(numFaces * 3);
	for (auto i = 0u; i < numFaces; i++) {
		const auto& face = faces[i];
		for (auto j = 0u; j < face.mNumIndices; j++) {
			indices.push_back(face.mIndices[j]);
		}
	}
}

void ModelLoader::readVerticesData(std::vector<SFE::Vertex3D>& vertices, unsigned numVertices, aiMesh* assimpMesh) {
	vertices.resize(numVertices);

	for (unsigned int i = 0; i < numVertices; i++) {
		auto& vertex = vertices[i];

		auto newPos =/* meshNode->mTransformation **/ assimpMesh->mVertices[i];
		// process vertex positions, normals and texture coordinates
		vertex.position.x = newPos.x;
		vertex.position.y = newPos.y;
		vertex.position.z = newPos.z;

		if (assimpMesh->mTextureCoords[0]) {
			vertex.texCoords.x = assimpMesh->mTextureCoords[0][i].x;
			vertex.texCoords.y = assimpMesh->mTextureCoords[0][i].y;
		}

		if (assimpMesh->mNormals) {
			vertex.normal.x = assimpMesh->mNormals[i].x;
			vertex.normal.y = assimpMesh->mNormals[i].y;
			vertex.normal.z = assimpMesh->mNormals[i].z;
		}

		if (assimpMesh->mTangents) {
			vertex.tangent.x = assimpMesh->mTangents[i].x;
			vertex.tangent.y = assimpMesh->mTangents[i].y;
			vertex.tangent.z = assimpMesh->mTangents[i].z;
		}

		if (assimpMesh->mBitangents) {
			vertex.biTangent.x = assimpMesh->mBitangents[i].x;
			vertex.biTangent.y = assimpMesh->mBitangents[i].y;
			vertex.biTangent.z = assimpMesh->mBitangents[i].z;
		}
		else if (assimpMesh->mTangents && assimpMesh->mNormals) {
			vertex.biTangent = SFE::Math::cross(vertex.normal, vertex.tangent);
		}
	}
}

int ModelLoader::extractLodLevel(const std::string& meshName) {
	if (meshName.size() <= 4) {
		return 0;
	}

	auto i = meshName.size() - 4;
	for (; i > 0; i--) {
		if (meshName[i] == '_' && meshName[i + 1] == 'L' && meshName[i + 2] == 'O' && meshName[i + 3] == 'D') {
			break;
		}
	}

	if (i == 0) {
		return 0;
	}

	return std::atoi(meshName.substr(i + 4, meshName.size() - i).c_str());
}

void ModelLoader::processMesh(aiMesh* assimpMesh, const aiScene* scene, const std::string& directory, SFE::MeshObject3D& meshObject, Armature& armature) {
	readVerticesData(meshObject.mesh.vertices, assimpMesh->mNumVertices, assimpMesh);
	readIndicesData(meshObject.mesh.indices, assimpMesh->mNumFaces, assimpMesh->mFaces);
	SFE::MeshUtils::generateTangents(&meshObject.mesh);
	readBonesData(meshObject.mesh.vertices, assimpMesh, armature);
	readMaterialData(meshObject.material, scene->mMaterials[assimpMesh->mMaterialIndex], directory);
}

std::vector<Texture*> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& directory) {
	std::vector<Texture*> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		std::string raw = str.C_Str();
		std::string filename = raw;
		if (const auto pos = raw.find_last_of("/\\"); pos != std::string::npos) {
			filename = raw.substr(pos + 1);
		}
		std::string relative = raw;
		std::replace(relative.begin(), relative.end(), '\\', '/');

		Texture* tex = nullptr;
		const std::string candidates[] = {
			directory + "/" + filename,
			directory + "/" + relative,
		};
		for (const auto& candidate : candidates) {
			if (!SFE::FileSystem::isFileExists(candidate)) {
				continue;
			}
			tex = TextureHandler::loadTexture(candidate);
			if (tex && tex->isValid()) {
				break;
			}
			tex = nullptr;
		}
		if (tex) {
			textures.emplace_back(tex);
		}
	}
	return textures;
}
