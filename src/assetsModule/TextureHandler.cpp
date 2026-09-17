#include "TextureHandler.h"

#include <stb_image.h>
#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

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

namespace {
	// An image read from its file, with what the loaders learn from its pixels.
	struct DecodedImage {
		unsigned char* data = nullptr; // RGBA8, stbi_image_free
		int width = 0;
		int height = 0;
		bool hasAlpha = false;
		bool hasCutoutAlpha = false;
		float fgMean = 0.f;
		// Where the RHI prepares textures on any thread, the texture itself, and no pixels any more.
		std::unique_ptr<SFE::Render::PreparedTexture> prepared;
	};

	// A texture loaded from a file: RGBA8 unless asked otherwise, with a full mip chain. Without the levels a
	// distant character samples one texel of a full size texture per pixel: the armour sparkles, and TAA averages
	// the sparkle into grey.
	SFE::Render::TextureDesc fileTextureDesc(const int width, const int height, const SFE::Render::TextureFormat format) {
		SFE::Render::TextureDesc desc;
		desc.width = static_cast<uint32_t>(width);
		desc.height = static_cast<uint32_t>(height);
		desc.format = format;
		desc.dimension = SFE::Render::TextureDimension::Tex2D;
		desc.mipLevels = SFE::Render::mipLevelsFor(desc.width, desc.height);
		return desc;
	}

	// Repeating, trilinear and anisotropic.
	SFE::Render::SamplerDesc fileSamplerDesc() {
		SFE::Render::SamplerDesc samplerDesc;
		samplerDesc.minFilter = SFE::Render::FilterMode::LinearMipmapLinear;
		samplerDesc.magFilter = SFE::Render::FilterMode::Linear;
		samplerDesc.addressU = SFE::Render::AddressMode::Repeat;
		samplerDesc.addressV = SFE::Render::AddressMode::Repeat;
		samplerDesc.addressW = SFE::Render::AddressMode::Repeat;
		// Without it a surface seen at a glancing angle picks the mip for the axis it is stretched along and
		// loses the detail on the other one, which looks like a smeared, low resolution texture. Both backends
		// clamp this to what the device allows. It is also the one piece of texture filtering the two implement
		// differently, so the frame comparison shows about 6% of pixels differing at the horizon rather than 1%.
		samplerDesc.maxAnisotropy = 16.f;
		return samplerDesc;
	}

	// The texture with its whole chain uploaded at once, on whatever thread calls it; see RenderHardwareInterface::prepareTexture.
	std::unique_ptr<SFE::Render::PreparedTexture> prepareWithMips(const SFE::Render::TextureDesc& desc, const unsigned char* data) {
		const auto chain = AssetsModule::TextureHandler::buildMipChain(data, static_cast<int>(desc.width), static_cast<int>(desc.height));
		std::vector<const void*> levels{ data };
		for (const auto& level : chain) {
			levels.push_back(level.data());
		}
		return SFE::Engine::instance()->rhi()->prepareTexture(desc, levels);
	}

	// Whether it is flipped is stb's flag for the calling thread.
	DecodedImage decodeImage(const std::string& path) {
		DecodedImage image;
		int channels = 0;
		image.data = stbi_load(SFE::FileSystem::resolve(path).string().c_str(), &image.width, &image.height, &channels, 4);
		if (!image.data) {
			return image;
		}
		const int n = image.width * image.height;
		double fgSum = 0.0;
		int fgCount = 0;
		int partialAlpha = 0;
		for (int i = 0; i < n; ++i) {
			const unsigned char* px = image.data + i * 4;
			if (px[3] < 250) {
				image.hasAlpha = true;
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
		image.hasCutoutAlpha = partialAlpha > static_cast<int>(n * 0.015);
		image.fgMean = fgCount > 0 ? static_cast<float>(fgSum / static_cast<double>(fgCount)) : 0.f;
		return image;
	}

	// Unflipped images predecode read ahead, waiting for loadTexture to take them.
	std::mutex gPredecodedMutex;
	std::unordered_map<std::string, DecodedImage> gPredecoded;
}

namespace {
	// Runs fn(i) for i in [0, count) on threads of their own. stb's flip flag is per thread from the first image
	// on, and these never read a flipped one; the calling thread's flag is left as it was.
	void forEachOnWorkers(const size_t count, const std::function<void(size_t)>& fn) {
		std::atomic<size_t> next{ 0 };
		std::vector<std::jthread> workers((std::min)(count, static_cast<size_t>((std::max)(1u, std::thread::hardware_concurrency()))));
		for (auto& worker : workers) {
			worker = std::jthread([&] {
				stbi_set_flip_vertically_on_load_thread(0);
				for (size_t i = next++; i < count; i = next++) {
					fn(i);
				}
			});
		}
	}
}

void TextureHandler::predecode(const std::vector<std::string>& paths) {
	std::vector<std::string> pending;
	{
		std::lock_guard lock(gPredecodedMutex);
		for (const auto& path : paths) {
			if (!AssetsManager::instance()->getAsset<Texture>(path) && !gPredecoded.contains(path)
				&& std::find(pending.begin(), pending.end(), path) == pending.end()) {
				pending.push_back(path);
			}
		}
	}
	forEachOnWorkers(pending.size(), [&](const size_t i) {
		auto image = decodeImage(pending[i]);
		if (!image.data) {
			return;
		}
		if (SFE::Engine::instance()->rhi()->preparesTextures()) {
			image.prepared = prepareWithMips(fileTextureDesc(image.width, image.height, SFE::Render::TextureFormat::RGBA8), image.data);
			if (image.prepared) {
				stbi_image_free(image.data);
				image.data = nullptr;
			}
		}
		std::lock_guard lock(gPredecodedMutex);
		gPredecoded.emplace(pending[i], std::move(image));
	});
}

Texture* TextureHandler::loadTextureArray(const std::string& name, const std::vector<std::string>& paths) {
	if (auto* texture = AssetsManager::instance()->getAsset<Texture>(name)) {
		return texture;
	}
	std::vector<DecodedImage> images(paths.size());
	std::vector<std::vector<std::vector<unsigned char>>> chains(paths.size());
	forEachOnWorkers(paths.size(), [&](const size_t i) {
		images[i] = decodeImage(paths[i]);
		chains[i] = buildMipChain(images[i].data, images[i].width, images[i].height);
	});
	const auto releasePixels = [&] {
		for (auto& image : images) {
			if (image.data) {
				stbi_image_free(image.data);
			}
		}
	};
	for (size_t i = 0; i < images.size(); ++i) {
		if (!images[i].data || images[i].width != images.front().width || images[i].height != images.front().height) {
			SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::texture array %s: %s is missing or not the size of the first layer", name.c_str(), paths[i].c_str());
			releasePixels();
			return &TextureHandler::instance()->mDefaultTex;
		}
	}

	auto desc = fileTextureDesc(images.front().width, images.front().height, SFE::Render::TextureFormat::RGBA8);
	desc.dimension = SFE::Render::TextureDimension::Array;
	desc.depth = static_cast<uint32_t>(images.size());
	desc.arrayLayers = desc.depth;
	auto* rhi = SFE::Engine::instance()->rhi();
	auto* texture = AssetsManager::instance()->createAsset<Texture>(name);
	texture->texture = rhi->createTexture(desc, nullptr);
	for (uint32_t layer = 0; layer < desc.depth; ++layer) {
		rhi->updateTexture(texture->texture, 0, 0, desc.width, desc.height, desc.format, images[layer].data, 0, layer);
		for (uint32_t level = 1; level < desc.mipLevels && level <= chains[layer].size(); ++level) {
			rhi->updateTexture(texture->texture, 0, 0, (std::max)(1u, desc.width >> level), (std::max)(1u, desc.height >> level), desc.format,
				chains[layer][level - 1].data(), level, layer);
		}
	}
	releasePixels();
	texture->sampler = rhi->createSampler(fileSamplerDesc());
	return texture;
}

void TextureHandler::dropPredecoded() {
	std::lock_guard lock(gPredecodedMutex);
	for (auto& [path, image] : gPredecoded) {
		if (image.data) {
			stbi_image_free(image.data);
		}
	}
	gPredecoded.clear();
}

namespace {
	// The whole chain, ours rather than the driver's. createTexture is given no pixels on purpose: with data it
	// would build the levels itself, and that is the part the two backends disagree on.
	SFE::Render::TextureHandle createWithMips(const SFE::Render::TextureDesc& desc, const unsigned char* data) {
		auto* rhi = SFE::Engine::instance()->rhi();
		// One upload for every level, where the RHI can: a texture and then each level after it is a wait apiece.
		if (data && rhi->preparesTextures()) {
			if (auto prepared = prepareWithMips(desc, data)) {
				return rhi->adoptTexture(std::move(prepared));
			}
		}
		const auto tex = rhi->createTexture(desc, nullptr);
		if (!tex || !data) {
			return tex;
		}
		rhi->updateTexture(tex, 0, 0, desc.width, desc.height, desc.format, data, 0);
		const auto levels = AssetsModule::TextureHandler::buildMipChain(data, static_cast<int>(desc.width), static_cast<int>(desc.height));
		for (size_t i = 0; i < levels.size(); ++i) {
			const uint32_t mip = static_cast<uint32_t>(i) + 1;
			rhi->updateTexture(tex, 0, 0, (std::max)(1u, desc.width >> mip), (std::max)(1u, desc.height >> mip),
				desc.format, levels[i].data(), mip);
		}
		return tex;
	}
}

std::vector<std::vector<unsigned char>> TextureHandler::buildMipChain(const unsigned char* rgba, int width, int height) {
	std::vector<std::vector<unsigned char>> levels;
	if (!rgba || width <= 0 || height <= 0) {
		return levels;
	}
	// A cut-out's coverage: the share of texels at or over the alpha test's 0.5. Averaging thins it level by level --
	// a pine needle a few texels wide is under the threshold after two -- until a tree far away is bare twigs. So
	// each level's alpha is scaled up until it covers as much as the image does, as the alpha test will see it.
	const auto histogram = [](const unsigned char* pixels, const size_t bytes) {
		std::array<size_t, 256> counts{};
		for (size_t i = 3; i < bytes; i += 4) {
			++counts[pixels[i]];
		}
		return counts;
	};
	const auto keepCoverage = [&](std::vector<unsigned char>& pixels, float target) {
		const auto counts = histogram(pixels.data(), pixels.size());
		const size_t want = static_cast<size_t>(target * static_cast<float>(pixels.size() / 4));
		// The lowest alpha that, lifted to 0.5, covers no more than wanted.
		size_t covered = 0;
		int threshold = 128;
		for (int a = 255; a > 0; --a) {
			if (covered + counts[a] > want) {
				break;
			}
			covered += counts[a];
			threshold = a;
		}
		if (threshold >= 128) {
			return;
		}
		const float scale = 128.f / static_cast<float>(threshold);
		for (size_t i = 3; i < pixels.size(); i += 4) {
			pixels[i] = static_cast<unsigned char>((std::min)(255.f, static_cast<float>(pixels[i]) * scale + 0.5f));
		}
	};
	float coverage = 0.f;
	{
		const auto counts = histogram(rgba, static_cast<size_t>(width) * height * 4);
		size_t over = 0;
		for (int a = 128; a < 256; ++a) {
			over += counts[a];
		}
		const size_t texels = static_cast<size_t>(width) * height;
		// Only an image with a real cut-out: at least 1% of it out, and something in.
		if (texels - over > texels / 100 && over > 0) {
			coverage = static_cast<float>(over) / static_cast<float>(texels);
		}
	}
	const unsigned char* src = rgba;
	int sw = width;
	int sh = height;
	while (sw > 1 || sh > 1) {
		const int dw = (std::max)(1, sw / 2);
		const int dh = (std::max)(1, sh / 2);
		std::vector<unsigned char> level(static_cast<size_t>(dw) * dh * 4);
		for (int y = 0; y < dh; ++y) {
			// An odd source side leaves a row or column over; the last destination texel takes it, which is
			// what both drivers' box filters do and keeps the edge from being dropped.
			const int y0 = (std::min)(y * 2, sh - 1);
			const int y1 = (std::min)(y0 + 1, sh - 1);
			for (int x = 0; x < dw; ++x) {
				const int x0 = (std::min)(x * 2, sw - 1);
				const int x1 = (std::min)(x0 + 1, sw - 1);
				const unsigned char* a = src + (static_cast<size_t>(y0) * sw + x0) * 4;
				const unsigned char* b = src + (static_cast<size_t>(y0) * sw + x1) * 4;
				const unsigned char* c = src + (static_cast<size_t>(y1) * sw + x0) * 4;
				const unsigned char* d = src + (static_cast<size_t>(y1) * sw + x1) * 4;
				unsigned char* out = level.data() + (static_cast<size_t>(y) * dw + x) * 4;
				for (int ch = 0; ch < 4; ++ch) {
					// +2 rounds to nearest, so a flat area keeps its exact value down the whole chain.
					out[ch] = static_cast<unsigned char>((a[ch] + b[ch] + c[ch] + d[ch] + 2) / 4);
				}
			}
		}
		if (coverage > 0.f) {
			keepCoverage(level, coverage);
		}
		levels.push_back(std::move(level));
		src = levels.back().data();
		sw = dw;
		sh = dh;
	}
	return levels;
}

Texture* TextureHandler::loadTexture(const std::string& path, bool flip, SFE::Render::TextureFormat format) {
	auto texture = AssetsManager::instance()->getAsset<Texture>(path);
	if (texture) {
		return texture;
	}

	DecodedImage image;
	bool predecoded = false;
	// Predecoded images are unflipped RGBA8, and a prepared texture is adopted on the device thread only.
	if (!flip && format == SFE::Render::TextureFormat::RGBA8 && SFE::Engine::isMainThread()) {
		std::lock_guard lock(gPredecodedMutex);
		if (const auto it = gPredecoded.find(path); it != gPredecoded.end()) {
			image = std::move(it->second);
			gPredecoded.erase(it);
			predecoded = true;
		}
	}
	if (!predecoded) {
		stbi_set_flip_vertically_on_load(flip);
		image = decodeImage(path);
	}
	const auto texWidth = image.width;
	const auto texHeight = image.height;
	const auto data = image.data;
	if (!data && !image.prepared) {
		SFE::LogsModule::Logger::LOG_ERROR("TextureHandler::can't load texture %s", path.c_str());
		return &TextureHandler::instance()->mDefaultTex;
	}

	texture = AssetsManager::instance()->createAsset<Texture>(path);
	texture->hasAlpha = image.hasAlpha;
	texture->hasCutoutAlpha = image.hasCutoutAlpha;
	texture->fgMean = image.fgMean;
	const auto desc = fileTextureDesc(texWidth, texHeight, format);

	const auto samplerDesc = fileSamplerDesc();


	if (image.prepared) {
		texture->texture = SFE::Engine::instance()->rhi()->adoptTexture(std::move(image.prepared));
		texture->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);
	}
	else if (SFE::Engine::isMainThread()) {
		texture->texture = createWithMips(desc, data);
		texture->sampler = SFE::Engine::instance()->rhi()->createSampler(samplerDesc);
		stbi_image_free(data);
	}
	else if (SFE::Render::activeGraphicsBackend() != SFE::Render::GraphicsBackend::OpenGL) {
		// The texture is made and uploaded right here; only turning it into a handle, which touches the resource
		// tables the frame reads, waits for the main thread.
		// Where the RHI cannot, all of it happens there. The task owns the pixels or the prepared texture, so leaving
		// the wait at shutdown cannot free them under it.
		auto* prepared = SFE::Engine::instance()->rhi()->preparesTextures() ? prepareWithMips(desc, data).release() : nullptr;
		if (prepared) {
			stbi_image_free(data);
		}
		auto fut = SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([texture, desc, data, prepared, samplerDesc]() {
			auto* rhi = SFE::Engine::instance()->rhi();
			if (prepared) {
				texture->texture = rhi->adoptTexture(std::unique_ptr<SFE::Render::PreparedTexture>(prepared));
			}
			else {
				texture->texture = createWithMips(desc, data);
				stbi_image_free(data);
			}
			texture->sampler = rhi->createSampler(samplerDesc);
		});
		if (fut.valid()) {
			while (fut.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
				if (!SFE::ThreadPool::isAlive() || SFE::ThreadPool::instance()->isShuttingDown()) {
					break;
				}
			}
		}
		else if (prepared) {
			delete prepared;
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
				tex->texture = createWithMips(desc, data);
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