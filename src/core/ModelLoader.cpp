#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "ecsModule/EntityManager.h"
#include "logsModule/logger.h"
#include "renderModule/TextureHandler.h"

using namespace Engine::CoreModule;

Engine::ModelModule::Model* ModelLoader::load(const std::string& path) {
	const auto it = std::ranges::find_if(models, [path](const std::pair<std::string, ModelModule::Model*>& a) {
		return a.first == path;
	});

	if (it != models.end()) {
		return it->second;
	}
	auto modelData = loadModel(path);
	/*if (modelData.mMeshes.empty()) {
		return nullptr;
	}*/

	models.emplace_back(path, ecsModule::ECSHandler::entityManagerInstance()->createEntity<ModelModule::Model>(modelData));
	return models.back().second;
}

void ModelLoader::releaseModel(const std::string& path) {
	const auto it = std::ranges::find_if(models, [path](const std::pair<std::string, ModelModule::Model*>& a) {
		return a.first == path;
	});

	if (it == models.end()) {
		return;
	}

	models.erase(it);
}

ModelLoader::~ModelLoader() {
	for (auto& model : models) {
		delete model.second;
	}
}

Engine::ModelModule::MeshNode ModelLoader::loadModel(const std::string& path) {
	RenderModule::TextureLoader loader;
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return {};
	}

	auto directory = path.substr(0, path.find_last_of('/'));

	ModelModule::MeshNode rawModel;
	processNode(scene->mRootNode, scene, &loader, directory, rawModel);

	return rawModel;
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::MeshNode& rawModel) {
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
		ModelModule::MeshNode* child = new ModelModule::MeshNode();
		rawModel.addElement(child);

		processNode(node->mChildren[i], scene, loader, directory, *child);
	}
}

void ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::MeshNode& rawModel) {
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

	rawModel.mMeshes[lodLevel].emplace_back();

	auto& modelMesh = rawModel.mMeshes[lodLevel].back();
	modelMesh = ModelModule::Mesh();
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

	std::vector<ModelModule::MaterialTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", loader, directory);
	assert(diffuseMaps.size() < 2);
	if (!diffuseMaps.empty()) {
		modelMesh.mMaterial.mDiffuse = diffuseMaps.front();
	}

	std::vector<ModelModule::MaterialTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", loader, directory);
	if (!specularMaps.empty()) {
		modelMesh.mMaterial.mSpecular = specularMaps.front();
	}

	std::vector<ModelModule::MaterialTexture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", loader, directory);
	if (!normalMaps.empty()) {
		modelMesh.mMaterial.mNormal = normalMaps.front();
	}

	modelMesh.bindMesh();
}

std::vector<Engine::ModelModule::MaterialTexture> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader, const std::string& directory) {
	std::vector<ModelModule::MaterialTexture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		//if(!loader->loadedTex.contains(directory + "/" + str.C_Str())){
		ModelModule::MaterialTexture texture;
		texture.mTexture = loader->loadTexture(directory + "/" + std::string(str.C_Str()));
		texture.mType = typeName;

		textures.push_back(texture);
		//}
	}
	return textures;
}
