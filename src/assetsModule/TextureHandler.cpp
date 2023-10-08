#include "TextureHandler.h"

#include <stb_image.h>
#include <string>

#include "AssetsManager.h"
#include "core/Engine.h"
#include "core/ThreadPool.h"
#include "glad/glad.h"
#include "logsModule/logger.h"

using namespace AssetsModule;

bool Texture::isValid() const {
	return this != nullptr && mId != std::numeric_limits<uint16_t>::max();
}

void TextureHandler::bindTexture(unsigned slot, unsigned type, unsigned id) {
	if (mBindedTextures[slot] == id) {
		return;
	}
	mBindedTextures[slot] = id;
	glActiveTexture(slot);
	glBindTexture(type, id);
}

Texture* TextureHandler::loadTexture(const std::string& path, bool flip) {
	auto texture = AssetsManager::instance()->getAsset<Texture>(path);
	if (texture) {
		return texture;
	}

	stbi_set_flip_vertically_on_load(flip);

	int texWidth, texHeight, nrChannels;
	auto data = stbi_load(path.data(), &texWidth, &texHeight, &nrChannels, 4);
	if (!data) {
		Engine::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", path.c_str());
		stbi_image_free(data);
		return &mDefaultTex;
	}

	unsigned texID = 0;

	texture = AssetsManager::instance()->createAsset<Texture>(path);

	if (Engine::UnnamedEngine::isMainThread()) {
		glGenTextures(1, &texID);

		TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, texID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}
	else {
		Engine::ThreadPool::instance()->addTaskToSynchronization([id = texture->assetId, data, texWidth, texHeight]()mutable {
			unsigned texID;

			glGenTextures(1, &texID);

			TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, texID);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);

			AssetsManager::instance()->getAsset<Texture>(id)->mId = texID;
		});
	}

	texture->mId = texID;
	texture->mPath = path;
	texture->mType = eTextureType::DEFAULT;
	return texture;
}

Texture* TextureHandler::loadCubemapTexture(const std::string& path, bool flip) {
	auto texture = AssetsManager::instance()->getAsset<Texture>(path);
	if (texture) {
		return texture;
	}

	stbi_set_flip_vertically_on_load(flip);

	unsigned int textureID;
	glGenTextures(1, &textureID);
	TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_CUBE_MAP, textureID);

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
	for (unsigned int i = 0; i < faces.size(); i++) {
		auto data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (!data) {
			Engine::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", faces[i].c_str());
			stbi_image_free(data);
			glDeleteTextures(1, &textureID);
			return {};
		}
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	texture = AssetsManager::instance()->createAsset<Texture>(path);

	texture->mId = textureID;
	texture->mPath = path;
	texture->mType = eTextureType::CUBEMAP;

	return texture;
}