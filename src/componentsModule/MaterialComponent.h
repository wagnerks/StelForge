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
		float opacity = 1.f;
		float ior = 1.5f;
		float roughness = 0.5f;
		float metallic = 0.f;
		Math::Vec3 tint{ 1.f, 1.f, 1.f };
	};

	inline MaterialComponent fromAssetMaterial(const SFE::Material& src) {
		MaterialComponent out;
		out.transparent = src.transparent;
		out.opacity = src.opacity;
		out.ior = src.ior;
		out.roughness = src.roughness;
		out.metallic = src.metallic;
		if (src.alphaCutout) {
			out.fragmentShader = DefaultGBufferMaskedFS;
		}
		for (const auto& [type, tex] : src.materialTextures) {
			if (!tex.texture) {
				continue;
			}
			if (src.alphaCutout && type == SFE::SUBSURFACE) {
				continue;
			}
			out.materials.addMaterial({ tex.uniformSlot, tex.texture });
		}
		return out;
	}
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::MaterialComponent> : std::true_type {};

using SFE::ComponentsModule::MaterialComponent;
