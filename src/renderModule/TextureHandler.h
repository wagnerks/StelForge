#pragma once

#include <map>
#include <unordered_map>

namespace GameEngine::RenderModule {

	class TextureLoader {
	public:
		std::unordered_map<std::string, unsigned> loadedTex;
		unsigned int loadTexture(const std::string& path, bool flip = false);
		unsigned int loadCubemapTexture(const std::string& path, bool flip = false);
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
