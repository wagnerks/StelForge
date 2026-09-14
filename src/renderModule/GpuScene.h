#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "assetsModule/modelModule/BoundingVolume.h"
#include "componentsModule/MaterialComponent.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render {
	class GpuMaterialTable;

	// What the GPU needs to cull and draw the opaque scene without the CPU touching it per frame: every
	// drawable submesh as an instance, sorted so that one draw group (pipeline + textures) is one indirect
	// call and one batch (a mesh inside a group) is one command in it. Rebuilt only when the set of
	// instances changes; per frame the CPU hands over the view's planes and nothing else.
	class GpuScene {
	public:
		struct DrawGroup {
			std::string vertexShader;
			std::string fragmentShader;
			ComponentsModule::Materials textures;
			uint64_t textureKey = 0;
			uint32_t firstBatch = 0;
			uint32_t batchCount = 0;
		};

		// The output of culling the scene for one view, drawn from the same frame's command list.
		struct View {
			BufferHandle visible;   // SfeInstance records, bound at 3 by the draw
			BufferHandle commands;  // one DrawIndexedArgs per batch
			DescriptorSetHandle set;
			size_t visibleCapacity = 0;
			size_t commandCapacity = 0;
			uint32_t boundIds[6]{};
		};

		void init();
		// Any thread.
		void markDirty() { mDirty.store(true, std::memory_order_release); }
		// Main thread, under the world read lock, before the frame is recorded.
		void update(GpuMaterialTable& materials);
		// Records the culling dispatch for `frustum` into `view`. Outside a render pass. False when there is
		// nothing to draw.
		bool cull(CommandListHandle cmd, View& view, const FrustumModule::Frustum& frustum, BufferHandle transforms);

		const std::vector<DrawGroup>& groups() const { return mGroups; }

		static constexpr uint32_t kCommandStride = 20;

	private:
		void rebuild(GpuMaterialTable& materials);
		void uploadMeshes();

		std::atomic<bool> mDirty{ true };
		std::vector<DrawGroup> mGroups;
		uint32_t mInstanceCount = 0;
		uint32_t mBatchCount = 0;

		BufferHandle mMeshes;
		BufferHandle mInstances;
		BufferHandle mBatches;
		size_t mMeshesUploaded = 0;
		size_t mMeshCapacity = 0;

		DescriptorSetLayoutHandle mCullLayout;
		PipelineHandle mCullPipeline;
	};
}
