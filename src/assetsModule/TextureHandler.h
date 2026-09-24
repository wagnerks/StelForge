#pragma once

#include <array>
#include <filesystem>
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
		// How it was read from its file, to read it again the same way (TextureHandler::reloadChanged).
		int width = 0;
		int height = 0;
		bool flip = false;
		SFE::Render::TextureFormat format = SFE::Render::TextureFormat::RGBA8;
	};

	class TextureHandler : public SFE::Singleton<TextureHandler> {
		friend Singleton;
	public:
		Texture mDefaultTex;

		// Any thread; the texture is on the device when it returns. One that does not read is mDefaultTex, which is
		// not isValid().
		static Texture* loadTexture(const std::string& path, bool flip = false, SFE::Render::TextureFormat format = SFE::Render::TextureFormat::RGBA8);
		// Image files read ahead, in parallel, for loadTexture (unflipped RGBA8) to upload without reading them again.
		// Whatever it did not take is let go with this.
		class Predecoded {
		public:
			Predecoded() = default;
			Predecoded(Predecoded&&) = default;
			Predecoded& operator=(Predecoded&&) = default; // over an empty one only
			Predecoded(const Predecoded&) = delete;
			Predecoded& operator=(const Predecoded&) = delete;
			~Predecoded();
		private:
			friend TextureHandler;
			std::vector<std::string> mPaths; // the ones read ahead here
		};
		[[nodiscard]] static Predecoded predecode(const std::vector<std::string>& paths);
		// A texture array, one layer per image in order, every image the same size; unflipped RGBA8 with mips.
		// Kept under `name`.
		static Texture* loadTextureArray(const std::string& name, const std::vector<std::string>& paths);
		// Every texture read from a file that has been written since, read again into the same texture: whatever draws
		// with it shows the new picture. One whose size or format changed is left as it was, and named in `unchanged`.
		// From the device thread, between frames. How many were read again.
		static size_t reloadChanged(std::string& unchanged);
		// Levels 1..n of an RGBA8 image, each a 2x2 box average of the one above it. Built here rather than by
		// the driver: OpenGL and Vulkan produce different chains, and the difference shows up at the horizon,
		// where a pixel on the alpha test threshold then flips from one backend to the other.
		static std::vector<std::vector<unsigned char>> buildMipChain(const unsigned char* rgba, int width, int height);
	};
}
