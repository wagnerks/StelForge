#include "texture.h"

#include "stb_image.h"
#include "glad/glad.h"
#include "logsModule/logger.h"
#include "renderModule/TextureHandler.h"

using namespace GameEngine;
using namespace GameEngine::Render;

unsigned Texture::getTextureID() const {
	return texID;
}

void Texture::bind(unsigned int slot) {
	this->slot = slot;
	RenderModule::TextureHandler::getInstance()->bindTexture(slot, GL_TEXTURE_2D, texID);
}

void Texture::unbind() {
	RenderModule::TextureHandler::getInstance()->bindTexture(slot, GL_TEXTURE_2D, 0);
}

Texture::~Texture() {
	clear();
}

void Texture::clear() {
	if (texID) {
		glDeleteTextures(1, &texID);
	}
}

unsigned int Texture::load(const std::string& path) {
	clear();
	return RenderModule::TextureHandler::getInstance()->loader.loadTexture(path, true);
}
