#include "GpuMaterialTable.h"

#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "renderModule/Batcher.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE::Render {
	namespace {
		// The rules the passes used to apply per draw, resolved once per material.
		GpuMaterial makeGpuMaterial(const DrawObject& obj) {
			GpuMaterial m;
			m.roughness = obj.roughness;
			m.metallic = obj.metallic;
			m.mapMask = obj.gBufferMapMask();
			m.alphaCutoff = obj.alphaDither ? 1.78f : obj.cutoutCutoff();
			m.uvSel = obj.gBufferUvSel();
			m.specFactor = obj.specFactor;
			m.clearcoat = obj.clearcoat;
			m.clearcoatRough = obj.clearcoatRough;
			m.tint[0] = obj.tint.x;
			m.tint[1] = obj.tint.y;
			m.tint[2] = obj.tint.z;
			m.opacity = obj.opacity > 0.f ? obj.opacity : 0.22f;
			m.ior = obj.ior > 1.f ? obj.ior : 1.52f;
			m.casterOpacity = (obj.opacity > 0.f && obj.opacity < 0.999f) ? obj.opacity : 1.f;
			m.shadowCutoff = obj.alphaFromMaps ? -0.18f : obj.cutoutCutoff();
			return m;
		}
	}

	uint32_t GpuMaterialTable::indexFor(const DrawObject& obj) {
		std::lock_guard lock(mMutex);
		const auto [it, inserted] = mIndexByHash.try_emplace(obj.materialHash, static_cast<uint32_t>(mEntries.size()));
		if (inserted) {
			mEntries.push_back(makeGpuMaterial(obj));
		}
		return it->second;
	}

	BufferHandle GpuMaterialTable::upload() {
		std::lock_guard lock(mMutex);
		auto* rhi = Engine::instance()->rhi();
		if (!mBuffer || mEntries.size() > mCapacity) {
			if (mBuffer) {
				// Frames already recorded may still read it.
				if (auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>()) {
					renderSystem->retireBuffer(mBuffer);
				}
				else {
					rhi->destroyBuffer(mBuffer);
				}
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
