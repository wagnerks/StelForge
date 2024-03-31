#pragma once

#include <array>
#include <map>
#include <string>
#include <unordered_map>

#include "Asset.h"
#include "containersModule/Singleton.h"
#include "glWrapper/Texture.h"

namespace AssetsModule {
	class Texture : public Asset {
	public:
		Texture(const Texture& other) = delete;
		Texture(Texture&& other) noexcept = delete;
		Texture& operator=(const Texture& other) = delete;
		Texture& operator=(Texture&& other) noexcept = delete;

		Texture() = default;
		Texture(SFE::GLW::TextureType type) : texture{ type } {}

		bool isValid() const;

		SFE::GLW::Texture texture;
	};

	class TextureHandler : public SFE::Singleton<TextureHandler> {
		friend Singleton;
	public:
		static void bindTextureToSlot(unsigned slot, Texture* texture);
		Texture mDefaultTex;

		static Texture* loadTexture(const std::string& path, bool flip = false, SFE::GLW::PixelFormat pixelFormat = SFE::GLW::RGBA8, SFE::GLW::TextureFormat textureFormat = SFE::GLW::RGBA, SFE::GLW::PixelDataType pixelType = SFE::GLW::UNSIGNED_BYTE);
		static Texture* loadCubemapTexture(const std::string& path, bool flip = false);
	};
}
