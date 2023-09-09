#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "Asset.h"
#include "core/Singleton.h"

namespace AssetsModule {

	enum class eTextureType {
		DEFAULT,
		CUBEMAP,
		TEXTURE_ARRAY
	};

	class Texture : public Asset {
	public:
		Texture() = default;
		Texture(unsigned id, std::string_view path, eTextureType type) : mId(id), mPath(path), mType(type) {}

		unsigned mId = std::numeric_limits<unsigned>::max();
		std::string mPath;
		eTextureType mType = eTextureType::DEFAULT;

		bool isValid() const {
			return mId != std::numeric_limits<unsigned>::max();
		}
	};

	class TextureLoader {
	public:
		Texture loadTexture(const std::string& path, bool flip = false);
		Texture loadCubemapTexture(const std::string& path, bool flip = false);
		Texture createEmpty2DTexture(const std::string& id, int w, int h, int format);
	private:
		std::unordered_map<std::string, Texture> mLoadedTex;
	};

	class TextureHandler : public Engine::Singleton<TextureHandler> {
		friend Singleton;
	public:
		void bindTexture(unsigned slot, unsigned type, unsigned int id);
		TextureLoader mLoader;
		unsigned getCurentTexture(unsigned slot) { return mBindedTextures[slot]; }
		Texture mDefaultTex;
	private:
		std::unordered_map<unsigned, unsigned> mBindedTextures;

	};
}
