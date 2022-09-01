#include "MaterialComponent.h"

void MaterialComponent::addTexture(const ModelModule::Material& texture) {
	/*if (std::ranges::find_if(textures, [texture](const ModelModule::ModelTexture& tex) {
		return texture.id == tex.id;
	}) == textures.end()) {
		textures.push_back(texture);
	}*/
	
}
void MaterialComponent::removeTexture(const ModelModule::Material& texture) {
	/*mMaterial = material;
	std::erase_if(textures, [texture](const ModelModule::ModelTexture& tex) {
		return texture.id == tex.id;
	});*/
	
}
