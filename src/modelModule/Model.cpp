#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/texture.h"
#include "logsModule/logger.h"
#include "renderModule/TextureHandler.h"


using namespace GameEngine::ModelModule;

void Model::draw(ShaderModule::ShaderBase* shader) {
    for(auto& mesh : meshes) {
		mesh.draw(shader);
	}
}

const std::vector<Mesh>& Model::getMeshes() {
	return meshes;
}

void Model::loadModel(const std::string& path) {
	RenderModule::TextureLoader loader;
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		LogsModule::Logger::LOG_ERROR("ASSIMP:: %s", import.GetErrorString());
		return;
	}

	directory = path.substr(0, path.find_last_of('/'));

	processNode(scene->mRootNode, scene, &loader);
}

void Model::processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader) {
	auto parent = node->mParent;
	while (parent) {
		node->mTransformation *= parent->mTransformation;
		parent = parent->mParent;
	}
	
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene, node, loader));
	}
		
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, loader);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader) {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<MeshTexture> textures;

	vertices.resize(mesh->mNumVertices);

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		auto& vertex = vertices[i];

		auto newPos = parent->mTransformation * mesh->mVertices[i];
		// process vertex positions, normals and texture coordinates
		vertex.Position.x = newPos.x;
		vertex.Position.y = newPos.y;
		vertex.Position.z = newPos.z;
		

		vertex.Normal.x = mesh->mNormals[i].x;
		vertex.Normal.y = mesh->mNormals[i].y;
		vertex.Normal.z = mesh->mNormals[i].z;

		if (mesh->mTextureCoords[0]) {
			vertex.TexCoords.x = mesh->mTextureCoords[0][i].x;
			vertex.TexCoords.y = mesh->mTextureCoords[0][i].y;
		}
	}
	// process indices
	indices.reserve(mesh->mNumFaces * 3);
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		const aiFace& face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	// process material
	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	std::vector<MeshTexture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", loader);
	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	std::vector<MeshTexture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", loader);
	textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

	return {vertices, indices, textures};
}

std::vector<MeshTexture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader) {
	std::vector<MeshTexture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		if(!loader->loadedTex.contains(directory + "/" + str.C_Str())){
			MeshTexture texture;
			texture.id = loader->loadTexture(directory + "/" + std::string(str.C_Str()));
			texture.type = typeName;
			textures.push_back(texture);
		}
	}
	return textures;
}