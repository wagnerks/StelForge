#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "core/Singleton.h"

namespace Engine::RenderModule {

	enum class eTextureType {
		DEFAULT,
		CUBEMAP,
		TEXTURE_ARRAY
	};

	struct Texture {
		unsigned mId = std::numeric_limits<unsigned>::max();
		std::string mPath;
		bool isValid() const {
			return mId != std::numeric_limits<unsigned>::max();
		}
		eTextureType mType = eTextureType::DEFAULT;
	};

	class TextureLoader {
	public:
		std::unordered_map<std::string, Texture> mLoadedTex;
		Texture loadTexture(const std::string& path, bool flip = false);
		Texture loadCubemapTexture(const std::string& path, bool flip = false);
		Texture createEmpty2DTexture(const std::string& id, int w, int h, int format);
	};

	class TextureHandler : public Singleton<TextureHandler> {
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
