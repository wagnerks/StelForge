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
	auto armature = model.second;

	std::vector<Animation> animations;
	animations.reserve(scene->mNumAnimations);
	for (auto i = 0u; i < scene->mNumAnimations; i++) {
		auto animation = scene->mAnimations[i];
		animations.emplace_back(animation);
	}

	mtx.lock();
	asset = AssetsManager::instance()->createAsset<Model>(path, std::move(model.first), path, model.second);
	asset->animations = std::move(animations);

	loading[path].notify_all();
	loading.erase(path);

	mtx.unlock();
	asset->normalizeModel();

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


ModelLoader::~ModelLoader() {
}

void ModelLoader::init() {}

std::pair<MeshNode, Armature> ModelLoader::loadModel(const aiScene* scene, const std::string& path) {
	auto directory = path.substr(0, path.find_last_of('/'));

	std::pair<MeshNode, Armature> res;
	//MeshNode rawModel;
	//Armature modelArmature;
	processNode(scene->mRootNode, scene, TextureHandler::instance(), directory, res.first, res.second);

	return res;
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene, TextureHandler* loader, const std::string& directory, MeshNode& rawModel, Armature& armature) {
	auto parent = node->mParent;
	//while (parent) {
	//	node->mTransformation *= parent->mTransformation;
	//	parent = parent->mParent;
	//}
	
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh(mesh, scene, node, loader, directory, rawModel, armature);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		auto child = new AssetsModule::MeshNode();
		rawModel.addElement(child);

		processNode(node->mChildren[i], scene, loader, directory, *child, armature);
	}
}

void ModelLoader::extractBones(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene, Armature& armature) {
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

void ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, aiNode* meshNode, AssetsModule::TextureHandler* loader, const std::string& directory, AssetsModule::MeshNode& rawModel, Armature& armature) {
	auto lodLevel = 0;
	{
		auto i = meshNode->mName.length - 4;
		for (; i > 0; i--) {
			if (meshNode->mName.data[i] == '_' && meshNode->mName.data[i + 1] == 'L' && meshNode->mName.data[i + 2] == 'O' && meshNode->mName.data[i + 3] == 'D') {
				break;
			}
		}

		if (i != 0) {
			auto nameString = std::string(meshNode->mName.C_Str());
			lodLevel = std::atoi(nameString.substr(i + 4, meshNode->mName.length - i).c_str());
		}
	}

	while (rawModel.mLods.size() <= lodLevel) {
		rawModel.mLods.emplace_back();
	}

	rawModel.mLods[lodLevel].emplace_back();
	
	auto& modelMesh = rawModel.mLods[lodLevel].back();
	modelMesh.mData.mVertices.resize(mesh->mNumVertices);
	{
		auto parent = meshNode->mParent;
		modelMesh.transform = assimpMatToMat4(meshNode->mTransformation);
		while (parent) {
			modelMesh.transform *= assimpMatToMat4(parent->mTransformation);
			parent = parent->mParent;
		}
	}

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		auto& vertex = modelMesh.mData.mVertices[i];

		auto newPos =/* meshNode->mTransformation **/ mesh->mVertices[i];
		// process vertex positions, normals and texture coordinates
		vertex.mPosition.x = newPos.x;
		vertex.mPosition.y = newPos.y;
		vertex.mPosition.z = newPos.z;

		if (mesh->mNormals) {
			vertex.mNormal.x = mesh->mNormals[i].x;
			vertex.mNormal.y = mesh->mNormals[i].y;
			vertex.mNormal.z = mesh->mNormals[i].z;
		}


		if (mesh->mTextureCoords[0]) {
			vertex.mTexCoords.x = mesh->mTextureCoords[0][i].x;
			vertex.mTexCoords.y = mesh->mTextureCoords[0][i].y;
		}

		if (mesh->mTangents) {
			vertex.mTangent.x = mesh->mTangents[i].x;
			vertex.mTangent.y = mesh->mTangents[i].y;
			vertex.mTangent.z = mesh->mTangents[i].z;
		}
		for (auto b = 0; b < 4; b++) {
			vertex.mBoneIDs[b] = -1;
			vertex.mWeights[b] = 0.f;
		}
	}

	// process indices
	modelMesh.mData.mIndices.reserve(mesh->mNumFaces * 3);
	for (auto i = 0u; i < mesh->mNumFaces; i++) {
		const auto& face = mesh->mFaces[i];
		for (auto j = 0u; j < face.mNumIndices; j++) {
			modelMesh.mData.mIndices.push_back(face.mIndices[j]);
		}
	}

	extractBones(modelMesh.mData.mVertices, mesh, scene, armature);

	// process material
	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

	std::vector<AssetsModule::MaterialTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", loader, directory);
	assert(diffuseMaps.size() < 2);
	if (!diffuseMaps.empty()) {
		modelMesh.mMaterial.mDiffuse = diffuseMaps.front();
	}

	std::vector<AssetsModule::MaterialTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		modelMesh.mMaterial.mSpecular = specularMaps.front();
	}

	std::vector<AssetsModule::MaterialTexture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", loader, directory);
	if (!normalMaps.empty()) {
		modelMesh.mMaterial.mNormal = normalMaps.front();
	}

	specularMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		modelMesh.mMaterial.mSpecular = specularMaps.front();
	}

	specularMaps = loadMaterialTextures(material, aiTextureType_EMISSIVE, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		modelMesh.mMaterial.mSpecular = specularMaps.front();
	}

	specularMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		modelMesh.mMaterial.mSpecular = specularMaps.front();
	}
	for (int i = aiTextureType_SHININESS; i <= aiTextureType_REFLECTION; i++) {
		specularMaps = loadMaterialTextures(material, (aiTextureType)i, "texture_specular", loader, directory);
		if (!specularMaps.empty()) {
			modelMesh.mMaterial.mSpecular = specularMaps.front();
		}
	}
	modelMesh.bindMesh();
}

std::vector<AssetsModule::MaterialTexture> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, AssetsModule::TextureHandler* loader, const std::string& directory) {
	std::vector<AssetsModule::MaterialTexture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		//if(!loader->loadedTex.contains(directory + "/" + str.C_Str())){
		AssetsModule::MaterialTexture texture;
		std::string path = std::string(str.C_Str());
		path.erase(0, std::string(str.C_Str()).find_last_of("\\") + 1);

		texture.mTexture = loader->loadTexture(directory + "/" + path);
		texture.mType = typeName;

		textures.push_back(texture);
		//}
	}
	return textures;
}
