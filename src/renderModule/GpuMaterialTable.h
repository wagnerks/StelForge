#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "componentsModule/MaterialComponent.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render {
	// Mirrors SfeMaterial in shaders/sfe_material.glsl.
	struct GpuMaterial {
		float roughness = 0.5f;
		float metallic = 0.f;
		float mapMask = 0.f;
		float alphaCutoff = 0.f;
		float uvSel = 0.f;
		float specFactor = 1.f;
		float clearcoat = 0.f;
		float clearcoatRough = 0.04f;
		float tint[3]{ 1.f, 1.f, 1.f };
		float opacity = 0.22f;
		float ior = 1.52f;
		float casterOpacity = 1.f;
		float shadowCutoff = 0.f;
		float pad = 0.f;
	};
	static_assert(sizeof(GpuMaterial) == 64, "must match the std430 SfeMaterial");

	// One entry per distinct material, keyed by MaterialRules::materialHash. Entries are appended and
	// never move, so an index handed out stays valid; the buffer holds them all on the GPU.
	class GpuMaterialTable {
	public:
		// Any thread.
		uint32_t indexFor(const ComponentsModule::MaterialComponent& material);
		// Main thread. Uploads entries added since the last call and returns the buffer to bind.
		BufferHandle upload();

	private:
		std::mutex mMutex;
		std::unordered_map<uint64_t, uint32_t> mIndexByHash;
		std::vector<GpuMaterial> mEntries;
		size_t mUploaded = 0;
		size_t mCapacity = 0;
		BufferHandle mBuffer;
	};
}
