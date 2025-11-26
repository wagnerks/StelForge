#pragma once

#include <array>
#include <map>
#include <string>
#include <unordered_map>

#include "Asset.h"
#include "containersModule/Singleton.h"
#include "glWrapper/Texture.h"
#include "renderModule/RenderHardwareInterface.h"

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
	};

	class TextureHandler : public SFE::Singleton<TextureHandler> {
		friend Singleton;
	public:
		static void bindTextureToSlot(unsigned slot, Texture* texture);
		Texture mDefaultTex;

		static Texture* loadTexture(const std::string& path, bool flip = false, SFE::Render::TextureFormat format = SFE::Render::TextureFormat::RGBA8);
		static Texture* loadCubemapTexture(const std::string& path, bool flip = false);
	};
}
