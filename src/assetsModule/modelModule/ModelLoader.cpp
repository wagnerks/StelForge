#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Animation.h"
#include "logsModule/logger.h"
#include "assetsModule/TextureHandler.h"
#include "core/ThreadPool.h"

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

	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		SFE::LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return nullptr;
	}
	
	auto model = loadModel(scene, path);

	std::vector<Animation> animations;
	animations.reserve(scene->mNumAnimations);
	for (auto i = 0u; i < scene->mNumAnimations; i++) {
		auto animation = scene->mAnimations[i];
		animations.emplace_back(animation);
	}

	mtx.lock();
	asset = AssetsManager::instance()->createAsset<Model>(path, std::move(model.first), std::move(model.second), std::move(animations));


	loading[path].notify_all();
	loading.erase(path);

	mtx.unlock();
	asset->recalculateNormals();

	return asset;
}

void ModelLoader::releaseModel(const std::string& path) {
	//const auto it = std::ranges::find_if(models, [path](const std::pair<std::string, ModelModule::Model*>& a) {
	//	return a.first == path;
	//});

	//if (it == models.end()) {
	//	return;
	//}

	//models.erase(it);
}


ModelLoader::~ModelLoader() {}

void ModelLoader::init() {}

std::pair<SFE::Tree<Mesh>, Armature> ModelLoader::loadModel(const aiScene* scene, const std::string& path) {
	auto directory = path.substr(0, path.find_last_of('/'));

	std::pair<SFE::Tree<Mesh>, Armature> res;

	processNode(scene->mRootNode, scene, TextureHandler::instance(), directory, res.first, res.second);

	return res;
}


void ModelLoader::processNode(aiNode* node, const aiScene* scene, TextureHandler* loader, const std::string& directory, SFE::Tree<Mesh>& rawModel, Armature& armature) {
	{
		rawModel.value.transform = assimpMatToMat4(node->mTransformation);

		auto parent = rawModel.parent;
		while (parent) {
			rawModel.value.transform *= parent->value.transform;
			parent = parent->parent;
		}
	}

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh(mesh, scene, node, loader, directory, rawModel.value, armature);
		rawModel.value.calculateBounds();
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		rawModel.addChild({});
		processNode(node->mChildren[i], scene, loader, directory, rawModel.children.front(), armature);
	}
}

void ModelLoader::readBonesData(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene, Armature& armature) {
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
				if (vertices[vertexId].mBoneIDs[i] < 0) {
					vertices[vertexId].mWeights[i] = weight;
					vertices[vertexId].mBoneIDs[i] = boneID;
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

void ModelLoader::readMaterialData(Material& material, aiMaterial* assimpMaterial, AssetsModule::TextureHandler* loader, const std::string& directory) {
	auto diffuseMaps = loadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE, loader, directory);
	assert(diffuseMaps.size() < 2);
	if (!diffuseMaps.empty()) {
		material[DIFFUSE] = diffuseMaps.front();
	}

	auto specularMaps = loadMaterialTextures(assimpMaterial, aiTextureType_SPECULAR, loader, directory);
	if (!specularMaps.empty()) {
		material[SPECULAR] = specularMaps.front();
	}

	auto normalMaps = loadMaterialTextures(assimpMaterial, aiTextureType_NORMALS, loader, directory);
	if (!normalMaps.empty()) {
		material[NORMALS] = normalMaps.front();
	}

	/*specularMaps = loadMaterialTextures(assimpMaterial, aiTextureType_HEIGHT, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		material.mSpecular = specularMaps.front();
	}

	specularMaps = loadMaterialTextures(assimpMaterial, aiTextureType_EMISSIVE, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		material.mSpecular = specularMaps.front();
	}

	specularMaps = loadMaterialTextures(assimpMaterial, aiTextureType_AMBIENT, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		material.mSpecular = specularMaps.front();
	}
	for (int i = aiTextureType_SHININESS; i <= aiTextureType_REFLECTION; i++) {
		specularMaps = loadMaterialTextures(assimpMaterial, (aiTextureType)i, "texture_specular", loader, directory);
		if (!specularMaps.empty()) {
			material.mSpecular = specularMaps.front();
		}
	}*/
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

void ModelLoader::readVerticesData(std::vector<Vertex>& vertices, unsigned numVertices, aiMesh* assimpMesh) {
	vertices.resize(numVertices);

	for (unsigned int i = 0; i < numVertices; i++) {
		auto& vertex = vertices[i];

		auto newPos =/* meshNode->mTransformation **/ assimpMesh->mVertices[i];
		// process vertex positions, normals and texture coordinates
		vertex.mPosition.x = newPos.x;
		vertex.mPosition.y = newPos.y;
		vertex.mPosition.z = newPos.z;

		if (assimpMesh->mTextureCoords[0]) {
			vertex.mTexCoords.x = assimpMesh->mTextureCoords[0][i].x;
			vertex.mTexCoords.y = assimpMesh->mTextureCoords[0][i].y;
		}

		if (assimpMesh->mNormals) {
			vertex.mNormal.x = assimpMesh->mNormals[i].x;
			vertex.mNormal.y = assimpMesh->mNormals[i].y;
			vertex.mNormal.z = assimpMesh->mNormals[i].z;
		}

		if (assimpMesh->mTangents) {
			vertex.mTangent.x = assimpMesh->mTangents[i].x;
			vertex.mTangent.y = assimpMesh->mTangents[i].y;
			vertex.mTangent.z = assimpMesh->mTangents[i].z;
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

void ModelLoader::processMesh(aiMesh* assimpMesh, const aiScene* scene, aiNode* meshNode, AssetsModule::TextureHandler* loader, const std::string& directory, AssetsModule::Mesh& mesh, Armature& armature) {
	mesh.lod = extractLodLevel(meshNode->mName.data);

	readVerticesData(mesh.mData.vertices, assimpMesh->mNumVertices, assimpMesh);
	readIndicesData(mesh.mData.indices, assimpMesh->mNumFaces, assimpMesh->mFaces);
	readBonesData(mesh.mData.vertices, assimpMesh, scene, armature);
	readMaterialData(mesh.mMaterial, scene->mMaterials[assimpMesh->mMaterialIndex], loader, directory);

	mesh.initMeshData();
}

std::vector<Texture*> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, AssetsModule::TextureHandler* loader, const std::string& directory) {
	std::vector<Texture*> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		//if(!loader->loadedTex.contains(directory + "/" + str.C_Str())){
		
		std::string path = std::string(str.C_Str());
		path.erase(0, std::string(str.C_Str()).find_last_of("\\") + 1);

		textures.emplace_back(loader->loadTexture(directory + "/" + path));
		//}
	}
	return textures;
}
