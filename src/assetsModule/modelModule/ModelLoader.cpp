#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "logsModule/logger.h"
#include "assetsModule/TextureHandler.h"
#include "core/ThreadPool.h"

using namespace AssetsModule;

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

	auto model = loadModel(path);
	mtx.lock();
	asset = AssetsManager::instance()->createAsset<Model>(path, std::move(model), path);

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

MeshNode ModelLoader::loadModel(const std::string& path) {
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		Engine::LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return {};
	}

	auto directory = path.substr(0, path.find_last_of('/'));

	MeshNode rawModel;
	processNode(scene->mRootNode, scene, TextureHandler::instance(), directory, rawModel);

	return rawModel;
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene, TextureHandler* loader, const std::string& directory, MeshNode& rawModel) {
	auto parent = node->mParent;
	while (parent) {
		node->mTransformation *= parent->mTransformation;
		parent = parent->mParent;
	}

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh(mesh, scene, node, loader, directory, rawModel);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		AssetsModule::MeshNode* child = new AssetsModule::MeshNode();
		rawModel.addElement(child);

		processNode(node->mChildren[i], scene, loader, directory, *child);
	}
}

void ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, AssetsModule::TextureHandler* loader, const std::string& directory, AssetsModule::MeshNode& rawModel) {
	auto lodLevel = 0;
	auto i = parent->mName.length - 4;
	for (; i > 0; i--) {
		if (parent->mName.data[i] == '_' && parent->mName.data[i + 1] == 'L' && parent->mName.data[i + 2] == 'O' && parent->mName.data[i + 3] == 'D') {
			break;
		}
	}

	if (i != 0) {
		auto nameString = std::string(parent->mName.C_Str());
		lodLevel = std::atoi(nameString.substr(i + 4, parent->mName.length - i).c_str());
	}

	while (rawModel.mMeshes.size() <= lodLevel) {
		rawModel.mMeshes.emplace_back();
	}

	rawModel.mMeshes[lodLevel].emplace_back();

	auto& modelMesh = rawModel.mMeshes[lodLevel].back();
	//modelMesh = AssetsModule::Mesh();
	modelMesh.mData.mVertices.resize(mesh->mNumVertices);

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		auto& vertex = modelMesh.mData.mVertices[i];

		auto newPos = parent->mTransformation * mesh->mVertices[i];
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
	}
	// process indices
	modelMesh.mData.mIndices.reserve(mesh->mNumFaces * 3);
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		const aiFace& face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			modelMesh.mData.mIndices.push_back(face.mIndices[j]);
	}

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
