#pragma once

#include <map>
#include <unordered_map>

namespace GameEngine::RenderModule {

	enum class eTextureType {
		NONE,
		TWO_D,
		CUBEMAP,
		TEXTURE_ARRAY
	};

	struct Texture {
		unsigned mId = std::numeric_limits<unsigned>::max();
		bool isValid() const {
			return mId != std::numeric_limits<unsigned>::max();
		}
		eTextureType mType = eTextureType::NONE;
	};

	class TextureLoader {
	public:
		std::unordered_map<std::string, Texture> loadedTex;
		Texture loadTexture(const std::string& path, bool flip = false);
		Texture loadCubemapTexture(const std::string& path, bool flip = false);
		Texture createEmpty2DTexture(const std::string& id, int w, int h, int format);
	};

	class TextureHandler {
	public:
		static TextureHandler* getInstance();
		void bindTexture(unsigned slot, unsigned type, unsigned int id);
		TextureLoader loader;
		unsigned getCurentTexture(unsigned slot) { return bindedTextures[slot];}
	private:
		std::unordered_map<unsigned, unsigned> bindedTextures;
		inline static TextureHandler* instance = nullptr;
	};
}
