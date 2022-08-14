#include "TextureComponent.h"

void TextureComponent::addTexture(const ModelModule::ModelTexture& texture) {
	if (std::ranges::find_if(textures, [texture](const ModelModule::ModelTexture& tex) {
		return texture.id == tex.id;
	}) == textures.end()) {
		textures.push_back(texture);
	}
	
}
void TextureComponent::removeTexture(const ModelModule::ModelTexture& texture) {
	std::erase_if(textures, [texture](const ModelModule::ModelTexture& tex) {
		return texture.id == tex.id;
	});
	
}
