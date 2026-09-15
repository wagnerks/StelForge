#include "GpuMaterialTable.h"

#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "logsModule/logger.h"
#include "renderModule/MaterialRules.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE::Render {
	namespace {
		// The rules the passes used to apply per draw, resolved once per material.
		GpuMaterial makeGpuMaterial(const ComponentsModule::MaterialComponent& src) {
			const float cutoff = MaterialRules::cutoutCutoff(src.alphaCutoff, src.alphaFromMaps, src.fragmentShader);
			GpuMaterial m;
			m.roughness = src.roughness;
			m.metallic = src.metallic;
			m.mapMask = MaterialRules::gBufferMapMask(src.materials, src.alphaFromMaps);
			m.alphaCutoff = src.alphaDither ? 1.78f : cutoff;
			m.uvSel = MaterialRules::gBufferUvSel(src.materials);
			m.specFactor = src.specFactor;
			m.clearcoat = src.clearcoat;
			m.clearcoatRough = src.clearcoatRough;
			m.tint[0] = src.tint.x;
			m.tint[1] = src.tint.y;
			m.tint[2] = src.tint.z;
			m.opacity = src.opacity > 0.f ? src.opacity : 0.22f;
			m.ior = src.ior > 1.f ? src.ior : 1.52f;
			m.casterOpacity = (src.opacity > 0.f && src.opacity < 0.999f) ? src.opacity : 1.f;
			m.shadowCutoff = src.alphaFromMaps ? -0.18f : cutoff;
			return m;
		}
	}

	uint32_t GpuMaterialTable::indexFor(const ComponentsModule::MaterialComponent& material) {
		const auto hash = MaterialRules::materialHash(material);
		std::lock_guard lock(mMutex);
		const auto [it, inserted] = mIndexByHash.try_emplace(hash, static_cast<uint32_t>(mEntries.size()));
		if (inserted) {
			auto entry = makeGpuMaterial(material);
			if (material.transparent) {
				entry.albedoSlot = static_cast<float>(glassSlot(MaterialRules::diffuseTexture(material.materials)));
			}
			mEntries.push_back(entry);
		}
		return it->second;
	}

	uint32_t GpuMaterialTable::glassSlot(const TextureHandle albedo) {
		for (size_t i = 0; i < mGlassTextures.size(); ++i) {
			if (mGlassTextures[i].id == albedo.id) {
				return static_cast<uint32_t>(i);
			}
		}
		if (mGlassTextures.size() == kGlassTextureSlots) {
			LogsModule::Logger::LOG_WARNING("GpuMaterialTable: more than %zu glass albedo textures, the rest draw untextured", kGlassTextureSlots);
			return 0;
		}
		mGlassTextures.push_back(albedo);
		return static_cast<uint32_t>(mGlassTextures.size() - 1);
	}

	std::vector<TextureHandle> GpuMaterialTable::glassTextures() {
		std::lock_guard lock(mMutex);
		return mGlassTextures;
	}

	BufferHandle GpuMaterialTable::upload() {
		std::lock_guard lock(mMutex);
		auto* rhi = Engine::instance()->rhi();
		if (!mBuffer || mEntries.size() > mCapacity) {
			if (mBuffer) {
				// Frames already recorded may still read it; the RHI keeps it until they have finished.
				rhi->destroyBuffer(mBuffer);
			}
			mCapacity = mCapacity ? mCapacity : 256;
			while (mCapacity < mEntries.size()) {
				mCapacity *= 2;
			}
			BufferDesc desc;
			desc.size = mCapacity * sizeof(GpuMaterial);
			desc.usage = BufferUsage::Storage;
			desc.cpuAccess = CpuAccess::None;
			desc.debugName = "GpuMaterialTable";
			mBuffer = rhi->createBuffer(desc);
			mUploaded = 0;
		}
		if (mUploaded < mEntries.size()) {
			rhi->updateBuffer(mBuffer, mUploaded * sizeof(GpuMaterial), (mEntries.size() - mUploaded) * sizeof(GpuMaterial), mEntries.data() + mUploaded);
			mUploaded = mEntries.size();
		}
		return mBuffer;
	}
}
