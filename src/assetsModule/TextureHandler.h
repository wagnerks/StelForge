#pragma once

#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "Asset.h"
#include "containersModule/Singleton.h"
#include "rhi/RenderHardwareInterface.h"

namespace AssetsModule {
	class Texture : public Asset {
	public:
		Texture(const Texture& other) = delete;
		Texture(Texture&& other) noexcept = delete;
		Texture& operator=(const Texture& other) = delete;
		Texture& operator=(Texture&& other) noexcept = delete;

		Texture() = default;

		bool isValid() const;

		SFE::Render::TextureHandle texture;
		SFE::Render::SamplerHandle sampler;
		bool hasAlpha = false;
		bool hasCutoutAlpha = false;
		float fgMean = 0.f;
	};

	class TextureHandler : public SFE::Singleton<TextureHandler> {
		friend Singleton;
	public:
		Texture mDefaultTex;

		static Texture* loadTexture(const std::string& path, bool flip = false, SFE::Render::TextureFormat format = SFE::Render::TextureFormat::RGBA8);
		// Reads these image files ahead, in parallel, for loadTexture (unflipped) to upload without reading them
		// again. dropPredecoded frees whatever was not asked for.
		static void predecode(const std::vector<std::string>& paths);
		// A texture array, one layer per image in order, every image the same size; unflipped RGBA8 with mips.
		// Kept under `name`.
		static Texture* loadTextureArray(const std::string& name, const std::vector<std::string>& paths);
		static void dropPredecoded();
		// Levels 1..n of an RGBA8 image, each a 2x2 box average of the one above it. Built here rather than by
		// the driver: OpenGL and Vulkan produce different chains, and the difference shows up at the horizon,
		// where a pixel on the alpha test threshold then flips from one backend to the other.
		static std::vector<std::vector<unsigned char>> buildMipChain(const unsigned char* rgba, int width, int height);
		static Texture* loadCubemapTexture(const std::string& path, bool flip = false);
	};
}
