#include "TextureHandler.h"

#include <stb_image.h>
#include <string>

#include "AssetsManager.h"
#include "core/Engine.h"
#include "core/ThreadPool.h"
#include "glad/glad.h"
#include "logsModule/logger.h"

using namespace AssetsModule;

void Texture::image2D(int width, int height, PixelFormat pixelFormat, TextureFormat format, PixelDataType type,	const void* data) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	glTexImage2D(mType, 0, pixelFormat, width, height, 0, format, type, data);
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

void Texture::image3D(int width, int height, int depth, PixelFormat pixelFormat, TextureFormat format, PixelDataType type,	const void* data) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	glTexImage3D(mType, 0, pixelFormat, width, height, depth, 0, format, type, data);
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

void Texture::bind() const {
	glBindTexture(mType, mId);
}

void Texture::setSubImageData2D(int xoffset, int yoffset, int w, int h, const void* data, TextureFormat format, PixelDataType type, bool bind) {
	unsigned previousId = 0;
	TextureType previousType = TEXTURE_2D;
	if (bind) {
		previousId = TextureHandler::instance()->getCurrentTexture(0);
		previousType = TextureHandler::instance()->getCurrentTextureType(0);
		TextureHandler::instance()->bindTextureToSlot(0, this);
	}
	
	glTexSubImage2D(mType, 0, xoffset, yoffset, w, h, format, type, data);

	if (bind) {
		TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
	}
}

void Texture::setParameter(TextureIParameter param, unsigned value) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	glTexParameteri(mType, param, value);
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

void Texture::setParameter(std::initializer_list<std::pair<TextureIParameter, unsigned>> parameters) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	for (auto& [param, value] : parameters) {
		glTexParameteri(mType, param, value);
	}
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

void Texture::setParameter(TextureFParameter param, float value) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	glTexParameterf(mType, param, value);
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

void Texture::setParameter(std::initializer_list<std::pair<TextureFParameter, float>> parameters) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	for (auto& [param, value] : parameters) {
		glTexParameterf(mType, param, value);
	}
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

void Texture::setPixelStorageMode(PixelStorageMode mode, PixelStorageModeValue value) {
	const auto previousId = TextureHandler::instance()->getCurrentTexture(0);
	const auto previousType = TextureHandler::instance()->getCurrentTextureType(0);
	TextureHandler::instance()->bindTextureToSlot(0, this);
	glPixelStorei(mode, static_cast<int>(value));
	TextureHandler::instance()->bindTextureToSlot(0, previousType, previousId);
}

bool Texture::isValid() const {
	return this != nullptr && mId != std::numeric_limits<uint16_t>::max();
}

void TextureHandler::bindTextureToSlot(unsigned slot, TextureType type, unsigned id) {
	if (mBindedTextures[slot].first == id) {
		return;
	}
	mBindedTextures[slot].first = id;
	setActiveTextureSlot(slot);
	glBindTexture(type, id);
}

void TextureHandler::bindTextureToSlot(unsigned slot, Texture* texture) {
	if (!texture) {
		return;
	}

	bindTextureToSlot(slot, texture->mType, texture->mId);
}

void TextureHandler::setActiveTextureSlot(unsigned slot) {
	glActiveTexture(TextureSlot(slot));
}

Texture* TextureHandler::loadTexture(const std::string& path, bool flip, PixelFormat pixelFormat, TextureFormat textureFormat, PixelDataType pixelType) {
	auto texture = AssetsManager::instance()->getAsset<Texture>(path);
	if (texture) {
		return texture;
	}

	stbi_set_flip_vertically_on_load(flip);

	int texWidth, texHeight, nrChannels;
	auto data = stbi_load(path.data(), &texWidth, &texHeight, &nrChannels, 4);
	if (!data) {
		SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", path.c_str());
		stbi_image_free(data);
		return &mDefaultTex;
	}

	unsigned texID = 0;

	texture = AssetsManager::instance()->createAsset<Texture>(path);

	if (SFE::Engine::isMainThread()) {
		glGenTextures(1, &texID);

		TextureHandler::instance()->bindTextureToSlot(0, TextureType::TEXTURE_2D, texID);

		glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(TextureType::TEXTURE_2D, 0, pixelFormat, texWidth, texHeight, 0, textureFormat, pixelType, data);

		stbi_image_free(data);
	}
	else {
		SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([id = texture->assetId, data, texWidth, texHeight, pixelFormat, textureFormat, pixelType]()mutable {
			unsigned texID;

			glGenTextures(1, &texID);

			TextureHandler::instance()->bindTextureToSlot(0, TextureType::TEXTURE_2D, texID);

			glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(TextureType::TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTexImage2D(TextureType::TEXTURE_2D, 0, pixelFormat, texWidth, texHeight, 0, textureFormat, pixelType, data);

			stbi_image_free(data);

			AssetsManager::instance()->getAsset<Texture>(id)->mId = texID;
		});
	}

	texture->mId = texID;
	texture->mType = TextureType::TEXTURE_2D;
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
	TextureHandler::instance()->bindTextureToSlot(0, TEXTURE_CUBE_MAP, textureID);

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
			SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", faces[i].c_str());
			stbi_image_free(data);
			glDeleteTextures(1, &textureID);
			return {};
		}
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	texture = AssetsManager::instance()->createAsset<Texture>(path);

	texture->mId = textureID;
	texture->mType = TextureType::TEXTURE_CUBE_MAP;

	return texture;
}