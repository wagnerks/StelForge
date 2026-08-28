#pragma once

#include <algorithm>
#include <string>

#include "mathModule/Forward.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::ComponentsModule {
	constexpr int MaxMaterialTexturesCount = 10;
	inline constexpr const char* DefaultGBufferVS = "shaders/g_buffer.vs";
	inline constexpr const char* DefaultGBufferFS = "shaders/g_buffer.fs";

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
		Math::Vec3 tint{ 1.f, 1.f, 1.f };
	};
}

using SFE::ComponentsModule::MaterialComponent;
