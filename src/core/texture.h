#pragma once
#include <string>

namespace GameEngine::Render {
	class Texture {
	public:
		unsigned int getTextureID() const;
		void bind(unsigned int slot);
		void unbind();

		~Texture();
	private:
		void clear();
		unsigned int load(const std::string& path);
		unsigned int texID = 0;
		unsigned slot = 0;
	};
}

