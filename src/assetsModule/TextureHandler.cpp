#include "TextureHandler.h"

#include <stb_image.h>
#include <algorithm>
#include <string>

#include "AssetsManager.h"
#include "core/FileSystem.h"
#include "rhi/GraphicsBackend.h"
#include "core/Engine.h"
#include "multithreading/ThreadPool.h"
#include "logsModule/logger.h"

using namespace AssetsModule;

bool Texture::isValid() const {
	return texture;
}

Texture* TextureHandler::loadTexture(const std::string& path, bool flip, SFE::Render::TextureFormat format) {
	auto texture = AssetsManager::instance()->getAsset<Texture>(path);
	if (texture) {
		return texture;
	}

	stbi_set_flip_vertically_on_load(flip);

	int texWidth, texHeight, nrChannels;
	const auto resolved = SFE::FileSystem::resolve(path).string();
	auto data = stbi_load(resolved.c_str(), &texWidth, &texHeight, &nrChannels, 4);
	if (!data) {
		SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", path.c_str());
		stbi_image_free(data);
		return &TextureHandler::instance()->mDefaultTex;
	}

	texture = AssetsManager::instance()->createAsset<Texture>(path);
	{
		const int n = texWidth * texHeight;
		double fgSum = 0.0;
		int fgCount = 0;
		int partialAlpha = 0;
		for (int i = 0; i < n; ++i) {
			const unsigned char* px = data + i * 4;
			if (px[3] < 250) {
				texture->hasAlpha = true;
			}
			if (px[3] > 8 && px[3] < 250) {
				++partialAlpha;
			}
			const float m = (std::max)(px[0], (std::max)(px[1], px[2])) / 255.f;
			if (m > 0.04f) {
				fgSum += m;
				++fgCount;
			}
		}
		texture->hasCutoutAlpha = partialAlpha > static_cast<int>(n * 0.015);
		texture->fgMean = fgCount > 0 ? static_cast<float>(fgSum / static_cast<double>(fgCount)) : 0.f;
	}
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
	samplerDesc.maxAnisotropy = 1.f;


	if (SFE::Engine::isMainThread()) {
		texture->texture = SFE::Engine::instance()->rhi()->createTexture(desc, data);
		texture->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);
		stbi_image_free(data);
	}
	else if (SFE::Render::activeGraphicsBackend() != SFE::Render::GraphicsBackend::OpenGL) {
		// Vulkan resources are created on the main thread, the way MeshVaoRegistry uploads meshes: creating a
		// texture submits an upload to the queue the frame submits to, and the resource pools are not synchronised.
		// The task frees the pixels, so leaving the wait at shutdown cannot free them under it.
		auto fut = SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([texture, desc, data, samplerDesc]() {
			texture->texture = SFE::Engine::instance()->rhi()->createTexture(desc, data);
			texture->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);
			stbi_image_free(data);
		});
		if (fut.valid()) {
			while (fut.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
				if (!SFE::ThreadPool::isAlive() || SFE::ThreadPool::instance()->isShuttingDown()) {
					break;
				}
			}
		}
		else {
			stbi_image_free(data);
		}
	}
	else {
		auto fut = SFE::ThreadPool::instance()->addTask<SFE::WorkerType::RESOURCE_LOADING>([id = texture->assetId, desc, data, samplerDesc]()mutable {
			auto tex = AssetsManager::instance()->getAsset<Texture>(id);

			auto* rhi = SFE::Engine::instance()->rhi();
			rhi->runOnWorkerThread([&] {
				tex->texture = rhi->createTexture(desc, data);
				tex->sampler = rhi->createSampler(samplerDesc);
			});

			stbi_image_free(data);
		});
		if (fut.valid()) {
			fut.wait();
		}
		else {
			stbi_image_free(data);
		}
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
		const auto resolved = SFE::FileSystem::resolve(faces[i]).string();
		data.push_back(stbi_load(resolved.c_str(), &width, &height, &nrChannels, 0));
		w = std::max(w, width);
		h = std::max(h, height);
	}

	desc.width = static_cast<uint32_t>(w);
	desc.height = static_cast<uint32_t>(h);

	for (unsigned int i = 0; i < faces.size(); i++) {
		const auto resolved = SFE::FileSystem::resolve(faces[i]).string();
		auto data = stbi_load(resolved.c_str(), &width, &height, &nrChannels, 0);
		if (!data) {
			SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", resolved.c_str());
			stbi_image_free(data);
			continue;
		}
		
		//texture->texture.image2D(static_cast<int>(CubeMapFaces::POSITIVE_X) + i, width, height,RGB8, data); //todo
		stbi_image_free(data);
	}

	
	return texture;
}