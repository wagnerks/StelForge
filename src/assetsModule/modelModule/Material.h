#pragma once
#include <unordered_map>
#include "rhi/RenderHardwareInterface.h"

namespace SFE {
	enum MaterialType : uint8_t {
		CUSTOM = 0,
		DIFFUSE = 1,
		NORMALS,
		SPECULAR,
		ROUGHNESS,
		METALLIC,
		SUBSURFACE
	};

	struct MaterialTexture {
		Render::TextureHandle texture;
		int uniformSlot = 0;
		MaterialType type = MaterialType::CUSTOM;
		int uvSet = 0;
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
		bool transparent = false;
		bool alphaCutout = false;
		bool alphaFromMaps = false;
		bool alphaDither = false;
		bool hasDiffuseAlpha = false;
		bool gltfPbr = false;
		float opacity = 1.f;
		float ior = 1.5f;
		float roughness = 0.5f;
		float metallic = 0.f;
		float alphaCutoff = 0.f;
		float specFactor = 1.f;
		float clearcoat = 0.f;
		float clearcoatRough = 0.04f;
	};
}
