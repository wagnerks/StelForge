#include "TextureHandler.h"

#include <stb_image.h>
#include <string>

#include "glad/glad.h"

using namespace GameEngine::RenderModule;

TextureHandler* TextureHandler::getInstance() {
	if (!instance) {
		instance = new TextureHandler();
	}

	return instance;
}

void TextureHandler::bindTexture(unsigned slot, unsigned type, unsigned id) {
	if (bindedTextures[slot] == id) {
		return;
	}
	bindedTextures[slot] = id;
	glActiveTexture(slot);
	glBindTexture(type, id);
}

unsigned TextureLoader::loadTexture(const std::string& path, bool flip) {
	if (&TextureHandler::getInstance()->loader == this) {
		auto it = loadedTex.find(path);
		if (it != loadedTex.end()) {
			return it->second;
		}
		stbi_set_flip_vertically_on_load(flip);
		unsigned texID;

		int texWidth, texHeight, nrChannels;

		auto data = stbi_load(path.data(), &texWidth, &texHeight, &nrChannels, 4);

		glGenTextures(1, &texID);

		TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, texID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);

		loadedTex[path] = texID;
		return texID;
	}

	auto it = loadedTex.find(path);
	if (it != loadedTex.end()) {
		return it->second;
	}
	auto id = TextureHandler::getInstance()->loader.loadTexture(path, flip);
	loadedTex[path] = id;
	return id;
}

unsigned TextureLoader::loadCubemapTexture(const std::string& path, bool flip) {
	if (&TextureHandler::getInstance()->loader == this) {
		auto it = loadedTex.find(path);
		if (it != loadedTex.end()) {
			return it->second;
		}

		stbi_set_flip_vertically_on_load(flip);

		unsigned int textureID;
		glGenTextures(1, &textureID);
		TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, textureID);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		std::vector<std::string> faces{
		    path + "right.jpg",
		    path + "left.jpg",
			path + "top.jpg",
			path + "bottom.jpg",
			path + "front.jpg",
			path + "back.jpg"
		};

		int width, height, nrChannels;
		for(unsigned int i = 0; i < faces.size(); i++) {
		    auto data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}

		loadedTex[path] = textureID;
		return textureID;
	}

	auto it = loadedTex.find(path);
	if (it != loadedTex.end()) {
		return it->second;
	}
	auto id = TextureHandler::getInstance()->loader.loadCubemapTexture(path, flip);
	loadedTex[path] = id;
	return id;
}
