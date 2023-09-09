#include "MaterialComponent.h"

void MaterialComponent::addTexture(const AssetsModule::Material& texture) {
	/*if (std::ranges::find_if(textures, [texture](const ModelModule::ModelTexture& tex) {
		return texture.id == tex.id;
	}) == textures.end()) {
		textures.push_back(texture);
	}*/

}
void MaterialComponent::removeTexture(const AssetsModule::Material& texture) {
	/*mMaterial = material;
	std::erase_if(textures, [texture](const ModelModule::ModelTexture& tex) {
		return texture.id == tex.id;
	});*/

}
