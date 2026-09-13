#pragma once

#include <algorithm>
#include <string>

#include "assetsModule/modelModule/Material.h"
#include "ecss/Types.h"
#include "mathModule/Forward.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::ComponentsModule {
	constexpr int MaxMaterialTexturesCount = 10;
	inline constexpr const char* DefaultGBufferVS = "shaders/g_buffer.vs";
	inline constexpr const char* DefaultGBufferFS = "shaders/g_buffer.fs";
	inline constexpr const char* DefaultGBufferMaskedFS = "shaders/g_buffer_masked.fs";

	struct MaterialData {
		int slot = 0;
		Render::TextureHandle texture;
		int uvSet = 0;
	};

	struct Materials {
		void addMaterial(MaterialData data) {
			material[materialsCount++] = std::move(data);
		}
		void clearMaterials() {
			materialsCount = 0;
		}

		MaterialData material[MaxMaterialTexturesCount];
		uint8_t materialsCount = 0;
	};

	struct MaterialComponent {
		Materials materials;
		std::string vertexShader = DefaultGBufferVS;
		std::string fragmentShader = DefaultGBufferFS;
		bool transparent = false;
		bool alphaFromMaps = false;
		bool alphaDither = false;
		float opacity = 1.f;
		float ior = 1.5f;
		float roughness = 0.5f;
		float metallic = 0.f;
		float alphaCutoff = 0.f;
		float specFactor = 1.f;
		float clearcoat = 0.f;
		float clearcoatRough = 0.04f;
		Math::Vec3 tint{ 1.f, 1.f, 1.f };
	};

	inline MaterialComponent fromAssetMaterial(const SFE::Material& src) {
		MaterialComponent out;
		out.transparent = src.transparent;
		out.alphaFromMaps = src.alphaFromMaps;
		out.alphaDither = src.alphaDither;
		out.opacity = src.opacity;
		out.ior = src.ior;
		out.roughness = src.roughness;
		out.metallic = src.metallic;
		out.alphaCutoff = src.alphaCutoff;
		out.specFactor = src.specFactor;
		out.clearcoat = src.clearcoat;
		out.clearcoatRough = src.clearcoatRough;
		if (src.alphaCutout) {
			out.fragmentShader = DefaultGBufferMaskedFS;
		}
		for (const auto& [type, tex] : src.materialTextures) {
			if (!tex.texture) {
				continue;
			}
			out.materials.addMaterial({ tex.uniformSlot, tex.texture, tex.uvSet });
		}
		return out;
	}
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::MaterialComponent> : std::true_type {};

using SFE::ComponentsModule::MaterialComponent;
