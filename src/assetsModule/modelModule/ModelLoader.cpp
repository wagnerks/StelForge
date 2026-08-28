#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Animation.h"
#include "MeshVaoRegistry.h"
#include "logsModule/logger.h"
#include "assetsModule/TextureHandler.h"
#include "core/FileSystem.h"
#include "multithreading/ThreadPool.h"

#include <algorithm>
#include <cctype>

using namespace AssetsModule;

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

	Assimp::Importer import;//todo create own format with aabb, materials, etc
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		SFE::LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return nullptr;
	}
	
	auto [meshes, armatur] = loadModel(scene, path);

	std::vector<Animation> animations;
	animations.reserve(scene->mNumAnimations);
	for (auto i = 0u; i < scene->mNumAnimations; i++) {
		auto animation = scene->mAnimations[i];
		animations.emplace_back(animation);
	}

	for (auto& meshNode : meshes) { //todo load it from model file, calculate edit, and add to model file
		auto& mesh = meshNode.value;
		auto minAABB = SFE::Math::Vec3(std::numeric_limits<float>::max());
		auto maxAABB = SFE::Math::Vec3(std::numeric_limits<float>::min());
		for (auto vertex : mesh.mesh.vertices) {
			vertex.position = mesh.transform * SFE::Math::Vec4(vertex.position, 1.f);
			minAABB.x = std::min(minAABB.x, vertex.position.x);
			minAABB.y = std::min(minAABB.y, vertex.position.y);
			minAABB.z = std::min(minAABB.z, vertex.position.z);

			maxAABB.x = std::max(maxAABB.x, vertex.position.x);
			maxAABB.y = std::max(maxAABB.y, vertex.position.y);
			maxAABB.z = std::max(maxAABB.z, vertex.position.z);
		}

		//mesh.aabb = SFE::FrustumModule::AABB(minAABB / 100.f, maxAABB / 100.f);
		mesh.aabb = SFE::FrustumModule::AABB(minAABB, maxAABB);
	}
	mtx.lock();
	asset = AssetsManager::instance()->createAsset<Model>(path, std::move(meshes), std::move(armatur), std::move(animations));


	loading[path].notify_all();
	loading.erase(path);

	mtx.unlock();
	

	return asset;
}

std::tuple<SFE::Tree<SFE::MeshObject3D>, Armature> ModelLoader::loadModel(const aiScene* scene, const std::string& path) {
	auto directory = path.substr(0, path.find_last_of('/'));

	SFE::Tree<SFE::MeshObject3D> meshes;
	Armature armat;

	processNode(scene->mRootNode, scene, directory, meshes, armat);

	return { meshes, armat };
}


void ModelLoader::processNode(aiNode* node, const aiScene* scene, const std::string& directory, SFE::Tree<SFE::MeshObject3D>& meshes, Armature& armature) {
	{
		meshes.value.transform = assimpMatToMat4(node->mTransformation);

		auto parent = meshes.parent;
		while (parent) {
			meshes.value.transform *= parent->value.transform;
			parent = parent->parent;
		}
	}

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		processMesh(scene->mMeshes[node->mMeshes[i]], scene, directory, meshes.value, armature);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		meshes.addChild({});
		processNode(node->mChildren[i], scene, directory, meshes.children.front(), armature);
	}
}

void ModelLoader::readBonesData(std::vector<SFE::Vertex3D>& vertices, aiMesh* mesh, const aiScene* scene, Armature& armature) {
	auto& bones = armature.bones;
	bones.reserve(mesh->mNumBones);

	std::map<std::string, int> bonesIds;

	for (const auto& bone : bones) {
		bonesIds[bone.name] = bone.id;
	}

	auto newBonesStart = bones.size();

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

	for (auto i = newBonesStart; i < bones.size(); i++){
		auto& bone = bones[i];
		if (auto node = scene->mRootNode->FindNode(bone.name.c_str())) {
			if (node->mParent) {
				std::string parentName = node->mParent->mName.C_Str();
				auto id = bonesIds.find(parentName);
				if(id != bonesIds.end()) {
					bone.parentBoneIdx = id->second;
				}
				else {
					armature.name = parentName;
					armature.transform = assimpMatToMat4(node->mParent->mParent->mTransformation * node->mParent->mTransformation);
				}
			}

			for (auto i = 0u; i < node->mNumChildren; i++) {
				if (auto child = node->mChildren[i]) {
					auto id = bonesIds.find(child->mName.C_Str());
					if (id != bonesIds.end()) {
						bone.childrenBones.push_back(id->second);
					}
				}
			}
		}
	}
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
		n + "_Base_Color.png", n + "_BaseColor.png", n + "_albedo.png",
		nUs + "_Base_Color.png", nUs + "_BaseColor.png",
		nSp + "_Base_Color.png", nSp + "_BaseColor.png"
	});
	tryFiles(SFE::NORMALS, {
		n + "_Normal.png", n + "_Normal_OpenGL.png",
		nUs + "_Normal.png", nSp + "_Normal.png"
	});
	tryFiles(SFE::ROUGHNESS, {
		n + "_Roughness.png", nUs + "_Roughness.png", nSp + "_Roughness.png"
	});
	tryFiles(SFE::METALLIC, {
		n + "_Metallic.png", nUs + "_Metallic.png", nSp + "_Metallic.png"
	});

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

	material.opacity = opacity;
	material.ior = ior > 1.f ? ior : 1.5f;
	material.transparent = nameGlass || transmission > 0.01f || (opacity > 0.001f && opacity < 0.99f);
}

void ModelLoader::readIndicesData(std::vector<unsigned>& indices, unsigned numFaces, aiFace* faces) {
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
	//meshObject.mesh.lod = extractLodLevel(meshNode->mName.data); //todo lods support, probably not throug model, but load it as separate meshes instead

	readVerticesData(meshObject.mesh.vertices, assimpMesh->mNumVertices, assimpMesh);
	readIndicesData(meshObject.mesh.indices, assimpMesh->mNumFaces, assimpMesh->mFaces);
	readBonesData(meshObject.mesh.vertices, assimpMesh, scene, armature);
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
