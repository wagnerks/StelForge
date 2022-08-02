#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "logsModule/logger.h"
#include "renderModule/TextureHandler.h"


GameEngine::ModelModule::Model* GameEngine::CoreModule::ModelLoader::load(const std::string& path) {
	if (auto it = loaded.find(path); it != loaded.end()) {
		return it->second.get();
	}
	auto meshes = loadModel(path);
	if (meshes.empty()) {
		return nullptr;
	}

	loaded[path] = std::make_unique<ModelModule::Model>(meshes);
	return loaded[path].get();
}

std::vector<std::unique_ptr<GameEngine::ModelModule::Mesh>> GameEngine::CoreModule::ModelLoader::loadModel(const std::string& path) {
	RenderModule::TextureLoader loader;
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return {};
	}

	auto directory = path.substr(0, path.find_last_of('/'));

	std::vector<std::unique_ptr<ModelModule::Mesh>> meshes;

	processNode(scene->mRootNode, scene, &loader, directory, meshes);

	return meshes;
}

void GameEngine::CoreModule::ModelLoader::processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader, const std::string& directory, std::vector<std::unique_ptr<ModelModule::Mesh>>& meshes) {
	auto parent = node->mParent;
	while (parent) {
		node->mTransformation *= parent->mTransformation;
		parent = parent->mParent;
	}
	
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.emplace_back(processMesh( mesh, scene, node, loader, directory));
	}
		
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, loader, directory, meshes);
	}
}

std::unique_ptr<GameEngine::ModelModule::Mesh> GameEngine::CoreModule::ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader, const std::string& directory) {
	std::unique_ptr<ModelModule::Mesh> modelMesh = std::make_unique<ModelModule::Mesh>();
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
	std::vector<ModelModule::MeshTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", loader, directory);
	modelMesh->textures.insert(modelMesh->textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	std::vector<ModelModule::MeshTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", loader, directory);
	modelMesh->textures.insert(modelMesh->textures.end(), specularMaps.begin(), specularMaps.end());
	std::vector<ModelModule::MeshTexture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", loader, directory);
	modelMesh->textures.insert(modelMesh->textures.end(), normalMaps.begin(), normalMaps.end());

	modelMesh->setupMesh();
	return modelMesh;
}

std::vector<GameEngine::ModelModule::MeshTexture> GameEngine::CoreModule::ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader, const std::string& directory) {
	std::vector<ModelModule::MeshTexture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		if(!loader->loadedTex.contains(directory + "/" + str.C_Str())){
			ModelModule::MeshTexture texture;
			texture.id = loader->loadTexture(directory + "/" + std::string(str.C_Str()));
			texture.type = typeName;
			textures.push_back(texture);
		}
	}
	return textures;
}
