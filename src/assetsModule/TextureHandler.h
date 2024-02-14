#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "Asset.h"
#include "core/Singleton.h"
#include "glad/glad.h"

namespace AssetsModule {

	enum class eTextureChannels {
		
	};

	enum class eTextureType {
		DEFAULT,
		CUBEMAP,
		TEXTURE_ARRAY
	};

	class Texture : public Asset {
	public:
		Texture(const Texture& other) = delete;
		Texture(Texture&& other) noexcept = delete;
		Texture& operator=(const Texture& other) = delete;
		Texture& operator=(Texture&& other) noexcept = delete;

		Texture() = default;
		Texture(unsigned id, std::string_view path, eTextureType type) : mId(id), mPath(path), mType(type) {}

		uint16_t mId = std::numeric_limits<uint16_t>::max();
		std::string mPath;
		eTextureType mType = eTextureType::DEFAULT;

		bool isValid() const;
	};

	class TextureHandler : public SFE::Singleton<TextureHandler> {
		friend Singleton;
	public:
		void bindTexture(unsigned slot, unsigned type, unsigned int id);
		unsigned getCurrentTexture(unsigned slot) { return mBindedTextures[slot]; }
		Texture mDefaultTex;

		Texture* loadTexture(const std::string& path, bool flip = false, unsigned type = GL_RGBA8, unsigned format = GL_RGBA);
		Texture* loadCubemapTexture(const std::string& path, bool flip = false);
	private:
		std::unordered_map<unsigned, unsigned> mBindedTextures;

	};
}
