#include "texture.h"

#include "logger.h"
#include "stb_image.h"
#include "glad/glad.h"

using namespace GameEngine;
using namespace GameEngine::Render;

unsigned Texture::loadTexture(const std::string& path) {
	stbi_set_flip_vertically_on_load(false);
	unsigned texID;

	int texWidth, texHeight, nrChannels;

	auto data = stbi_load(path.c_str(), &texWidth, &texHeight, &nrChannels, 4);

	glGenTextures(1, &texID);

	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	return texID;
}

void Texture::init(const std::string& path) {
	data = load(path);
}

unsigned Texture::getTextureID() const {
	return texID;
}

void Texture::bind(unsigned int slot) {
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, texID);
}

void Texture::unbind() {
	glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture() {
	clear();
}

void Texture::clear() {
	if (texID) {
		stbi_image_free(data);
		glDeleteTextures(1, &texID);
	}
}

unsigned char* Texture::load(const std::string& path) {
	clear();

	stbi_set_flip_vertically_on_load(true);

	texturePath = path;

	data = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &nrChannels, 4);

	glGenTextures(1, &texID);

	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	unbind();

	//glGenerateMipmap(GL_TEXTURE_2D);
	
	if (!data) {
		Logger::LOG_WARNING("Can't load texture %s", path.c_str());
	}
	
	return data;
}
