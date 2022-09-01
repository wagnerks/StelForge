#include "TextureHandler.h"

#include <stb_image.h>
#include <string>

#include "glad/glad.h"
#include "logsModule/logger.h"

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

Texture TextureLoader::loadTexture(const std::string& path, bool flip) {
	if (&TextureHandler::getInstance()->loader == this) {
		auto it = loadedTex.find(path);
		if (it != loadedTex.end()) {
			return it->second;
		}
		stbi_set_flip_vertically_on_load(flip);
		unsigned texID;

		int texWidth, texHeight, nrChannels;

		auto data = stbi_load(path.data(), &texWidth, &texHeight, &nrChannels, 4);
		if (!data) {
			LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", path.c_str());
			stbi_image_free(data);
			return {};
		}
		glGenTextures(1, &texID);

		TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, texID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);

		Texture tex = {texID, eTextureType::TWO_D};
		loadedTex[path] = tex;
		return tex;
	}

	auto it = loadedTex.find(path);
	if (it != loadedTex.end()) {
		return it->second;
	}
	auto id = TextureHandler::getInstance()->loader.loadTexture(path, flip);
	loadedTex[path] = id;
	return id;
}

Texture TextureLoader::loadCubemapTexture(const std::string& path, bool flip) {
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
			if (!data) {
				LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", faces[i].c_str());
				stbi_image_free(data);
				glDeleteTextures(1, &textureID);
				return {};
			}
		    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}

		Texture tex = {textureID, eTextureType::CUBEMAP};
		loadedTex[path] = tex;
		return tex;
	}

	auto it = loadedTex.find(path);
	if (it != loadedTex.end()) {
		return it->second;
	}
	auto id = TextureHandler::getInstance()->loader.loadCubemapTexture(path, flip);
	loadedTex[path] = id;
	return id;
}

Texture TextureLoader::createEmpty2DTexture(const std::string& id, int w, int h, int format) {
	unsigned int textureID;
	glGenTextures(1, &textureID);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, nullptr);

	return {textureID, eTextureType::TWO_D};
}
