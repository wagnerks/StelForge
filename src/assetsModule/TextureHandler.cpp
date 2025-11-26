#include "TextureHandler.h"

#include <stb_image.h>
#include <string>

#include "AssetsManager.h"
#include "core/Engine.h"
#include "multithreading/ThreadPool.h"
#include "logsModule/logger.h"

using namespace AssetsModule;

bool Texture::isValid() const {
	return texture;
}

void TextureHandler::bindTextureToSlot(unsigned slot, Texture* texture) {
	if (!texture) {
		return;
	}

	SFE::Engine::instance()->rhi()->bindTexture(slot, texture->texture, texture->sampler);
}

Texture* TextureHandler::loadTexture(const std::string& path, bool flip, SFE::Render::TextureFormat format) {
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
	SFE::Render::TextureDesc desc;
	desc.width = static_cast<uint32_t>(texWidth);
	desc.height = static_cast<uint32_t>(texHeight);
	desc.format = format;
	desc.dimension = SFE::Render::TextureDimension::Tex2D;

	SFE::Render::SamplerDesc samplerDesc;
	samplerDesc.minFilter = SFE::Render::FilterMode::Linear;
	samplerDesc.magFilter = SFE::Render::FilterMode::Linear;
	samplerDesc.addressU = SFE::Render::AddressMode::Repeat;
	samplerDesc.addressV = SFE::Render::AddressMode::Repeat;
	samplerDesc.addressW = SFE::Render::AddressMode::Repeat;


	if (SFE::Engine::isMainThread()) {
		texture->texture = SFE::Engine::instance()->rhi()->createTexture(desc, data);
		texture->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);

		stbi_image_free(data);
	}
	else {
		SFE::ThreadPool::instance()->addTask<SFE::WorkerType::RESOURCE_LOADING>([id = texture->assetId, desc, data, samplerDesc]()mutable {
			auto tex = AssetsManager::instance()->getAsset<Texture>(id);

			tex->texture = SFE::Engine::instance()->rhi()->createTexture(desc, data);
			tex->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);

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
	//todo
	stbi_set_flip_vertically_on_load(flip);

	texture = AssetsManager::instance()->createAsset<Texture>(path);

	SFE::Render::TextureDesc desc;
	desc.dimension = SFE::Render::TextureDimension::Cube;

	SFE::Render::SamplerDesc samplerDesc;
	samplerDesc.minFilter = SFE::Render::FilterMode::Linear;
	samplerDesc.magFilter = SFE::Render::FilterMode::Linear;
	samplerDesc.addressU = SFE::Render::AddressMode::ClampToEdge;
	samplerDesc.addressV = SFE::Render::AddressMode::ClampToEdge;
	samplerDesc.addressW = SFE::Render::AddressMode::ClampToEdge;


	texture->texture = SFE::Engine::instance()->rhi()->createTexture(desc);
	texture->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);


	std::vector<std::string> faces{
		path + "right.jpg",
		path + "left.jpg",
		path + "top.jpg",
		path + "bottom.jpg",
		path + "front.jpg",
		path + "back.jpg"
	};
	std::vector<stbi_uc*> data;
	int w, h;
	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++) {
		data.push_back(stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0));
		w = std::max(w, width);
		h = std::max(h, height);
	}

	desc.width = static_cast<uint32_t>(w);
	desc.height = static_cast<uint32_t>(h);

	for (unsigned int i = 0; i < faces.size(); i++) {
		auto data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (!data) {
			SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", faces[i].c_str());
			stbi_image_free(data);
			continue;
		}
		
		//texture->texture.image2D(static_cast<int>(SFE::GLW::CubeMapFaces::POSITIVE_X) + i, width, height, SFE::GLW::RGB8, SFE::GLW::RGB, SFE::GLW::UNSIGNED_BYTE, data); //todo
		stbi_image_free(data);
	}

	
	return texture;
}