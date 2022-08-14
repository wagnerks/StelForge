#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "ecsModule/EntityManager.h"
#include "logsModule/logger.h"
#include "renderModule/TextureHandler.h"

using namespace GameEngine::CoreModule;

GameEngine::ModelModule::Model* ModelLoader::load(const std::string& path) {
	if (auto it = models.find(path); it != models.end()) {
		return it->second.get();
	}
	auto modelData = loadModel(path);
	if (modelData.meshes.empty()) {
		return nullptr;
	}

	models[path] = std::make_unique<ModelModule::Model>(modelData);
	return models[path].get();
}

void ModelLoader::releaseModel(const std::string& path) {
	auto it = models.find(path);
	if (it == models.end()) {
		return;
	}

	models.erase(it);
}

GameEngine::ModelModule::RawModel ModelLoader::loadModel(const std::string& path) {
	RenderModule::TextureLoader loader;
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FixInfacingNormals);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return {};
	}

	auto directory = path.substr(0, path.find_last_of('/'));

	ModelModule::RawModel rawModel;
	processNode(scene->mRootNode, scene, &loader, directory, rawModel);

	return rawModel;
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::RawModel& rawModel) {
	auto parent = node->mParent;
	while (parent) {
		node->mTransformation *= parent->mTransformation;
		parent = parent->mParent;
	}
	
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh( mesh, scene, node, loader, directory, rawModel);
	}
		
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, loader, directory, rawModel);
	}
}

void ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::RawModel& rawModel) {
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
	
	
	rawModel.meshes[lodLevel].emplace_back();

	auto& modelMesh = rawModel.meshes[lodLevel].back();
	modelMesh = std::make_unique<ModelModule::Mesh>();
	modelMesh->vertices.resize(mesh->mNumVertices);

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		auto& vertex = modelMesh->vertices[i];

		auto newPos = parent->mTransformation * mesh->mVertices[i];
		// process vertex positions, normals and texture coordinates
		vertex.Position.x = newPos.x;
		vertex.Position.y = newPos.y;
		vertex.Position.z = newPos.z;
		
		if (mesh->mNormals) {
			vertex.Normal.x = mesh->mNormals[i].x;
			vertex.Normal.y = mesh->mNormals[i].y;
			vertex.Normal.z = mesh->mNormals[i].z;
		}
		

		if (mesh->mTextureCoords[0]) {
			vertex.TexCoords.x = mesh->mTextureCoords[0][i].x;
			vertex.TexCoords.y = mesh->mTextureCoords[0][i].y;
		}

		if (mesh->mTangents) {
			vertex.Tangent.x = mesh->mTangents[i].x;
			vertex.Tangent.y = mesh->mTangents[i].y;
			vertex.Tangent.z = mesh->mTangents[i].z;
		}
	}
	// process indices
	modelMesh->indices.reserve(mesh->mNumFaces * 3);
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		const aiFace& face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			modelMesh->indices.push_back(face.mIndices[j]);
	}

	// process material
	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	std::vector<GameEngine::ModelModule::ModelTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", loader, directory);
	rawModel.textures.insert(rawModel.textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	std::vector<GameEngine::ModelModule::ModelTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", loader, directory);
	rawModel.textures.insert(rawModel.textures.end(), specularMaps.begin(), specularMaps.end());
	std::vector<GameEngine::ModelModule::ModelTexture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", loader, directory);
	rawModel.textures.insert(rawModel.textures.end(), normalMaps.begin(), normalMaps.end());

	modelMesh->setupMesh();
}

std::vector<GameEngine::ModelModule::ModelTexture> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader, const std::string& directory) {
	std::vector<GameEngine::ModelModule::ModelTexture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		if(!loader->loadedTex.contains(directory + "/" + str.C_Str())){
			GameEngine::ModelModule::ModelTexture texture;
			texture.id = loader->loadTexture(directory + "/" + std::string(str.C_Str()));
			texture.type = typeName;
			textures.push_back(texture);
		}
	}
	return textures;
}
