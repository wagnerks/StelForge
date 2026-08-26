#pragma once
#include <unordered_map>
#include "renderModule/RenderHardwareInterface.h"

namespace SFE {
	enum MaterialType : uint8_t {
		CUSTOM = 0,
		DIFFUSE = 1,
		NORMALS,
		SPECULAR
	};

	struct MaterialTexture {
		Render::TextureHandle texture;
		int uniformSlot = 0;
		MaterialType type = MaterialType::CUSTOM;
	};

	struct Material {
		MaterialTexture& operator[](MaterialType type) { return materialTextures[type]; }
		MaterialTexture* tryGetTexture(MaterialType type) {
			const auto it = materialTextures.find(type);

			if (it != materialTextures.cend()) {
				return &it->second;
			}

			return nullptr;
		}


		std::unordered_map<MaterialType, MaterialTexture> materialTextures;
	};
}
