#include "TextureHandler.h"

#include <stb_image.h>
#include <string>

#include "AssetsManager.h"
#include "core/Engine.h"
#include "multithreading/ThreadPool.h"
#include "logsModule/logger.h"

using namespace AssetsModule;

bool Texture::isValid() const {
	return texture.isValid();
}

void TextureHandler::bindTextureToSlot(unsigned slot, Texture* texture) {
	if (!texture) {
		return;
	}

	SFE::GLW::bindTextureToSlot(slot, texture->texture.mType, texture->texture.mId);
}

Texture* TextureHandler::loadTexture(const std::string& path, bool flip, SFE::GLW::PixelFormat pixelFormat, SFE::GLW::TextureFormat textureFormat, SFE::GLW::PixelDataType pixelType) {
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
		return &TextureHandler::instance()->mDefaultTex;
	}

	texture = AssetsManager::instance()->createAsset<Texture>(path);
	texture->texture.mType = SFE::GLW::TextureType::TEXTURE_2D;

	texture->texture.parameters.minFilter = SFE::GLW::TextureMinFilter::LINEAR;
	texture->texture.parameters.magFilter = SFE::GLW::TextureMagFilter::LINEAR;

	texture->texture.parameters.wrap.S = SFE::GLW::TextureWrap::REPEAT;
	texture->texture.parameters.wrap.T = SFE::GLW::TextureWrap::REPEAT;

	texture->texture.pixelFormat = pixelFormat;
	texture->texture.textureFormat = textureFormat;
	texture->texture.pixelType = pixelType;
	texture->texture.width = texWidth;
	texture->texture.height = texHeight;

	if (SFE::Engine::isMainThread()) {
		texture->texture.create(data);
		stbi_image_free(data);
	}
	else {
		SFE::ThreadPool::instance()->addTask<SFE::WorkerType::RESOURCE_LOADING>([id = texture->assetId, data]()mutable {
			AssetsManager::instance()->getAsset<Texture>(id)->texture.create(data);
			stbi_image_free(data);
		});
	}

	return texture;
}

Texture* TextureHandler::loadCubemapTexture(const std::string& path, bool flip) {
	auto texture = AssetsManager::instance()->getAsset<Texture>(path);
	if (texture) {
		return texture;
	}

	stbi_set_flip_vertically_on_load(flip);

	texture = AssetsManager::instance()->createAsset<Texture>(path);
	texture->texture.mType = SFE::GLW::TextureType::TEXTURE_CUBE_MAP;
	texture->texture.parameters.magFilter = SFE::GLW::TextureMagFilter::LINEAR;
	texture->texture.parameters.minFilter = SFE::GLW::TextureMinFilter::LINEAR;
	texture->texture.parameters.wrap.S = SFE::GLW::TextureWrap::CLAMP_TO_EDGE;
	texture->texture.parameters.wrap.T = SFE::GLW::TextureWrap::CLAMP_TO_EDGE;
	texture->texture.parameters.wrap.R = SFE::GLW::TextureWrap::CLAMP_TO_EDGE;
	texture->texture.generate();


	SFE::GLW::bindTextureToSlot(0, texture->texture.mType, texture->texture.mId);
	texture->texture.parameters.apply(&texture->texture);
	texture->texture.applyPixelStorageMode();

	

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
			continue;
		}
		
		texture->texture.image2D(static_cast<int>(SFE::GLW::CubeMapFaces::POSITIVE_X) + i, width, height, SFE::GLW::RGB8, SFE::GLW::RGB, SFE::GLW::UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}

	SFE::GLW::bindTexture(texture->texture.mType, 0);
	
	return texture;
}