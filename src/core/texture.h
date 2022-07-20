#pragma once
#include <string>

namespace GameEngine::Render {
	class Texture {
	public:
		static unsigned int loadTexture(const std::string& path);
		void init(const std::string& path);
		unsigned int getTextureID() const;
		void bind(unsigned int slot);
		void unbind();

		~Texture();
	private:
		void clear();
		unsigned char* load(const std::string& path);
		unsigned char* data = nullptr;
		unsigned int texID = 0;
		std::string texturePath;
		int texWidth = 0;
		int texHeight = 0;
		int nrChannels = 0;
	};
}

