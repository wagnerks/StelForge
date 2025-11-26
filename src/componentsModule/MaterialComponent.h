#pragma once

#include <algorithm>

#include "renderModule/RenderHardwareInterface.h"

namespace SFE::ComponentsModule {
	constexpr int MaxMaterialTexturesCount = 10;

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
	};
}

using SFE::ComponentsModule::MaterialComponent;