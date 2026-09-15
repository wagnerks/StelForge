#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "assetsModule/RenderMeshData.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "componentsModule/MaterialComponent.h"
#include "rhi/RenderHardwareInterface.h"

namespace SFE::Render {
	class GpuMaterialTable;

	// What the GPU needs to cull and draw the scene without the CPU touching it per frame: every drawable
	// submesh as an instance. One draw group is one indirect call and one batch (a mesh inside a group) is
	// one command in it. Instances are added and removed per entity as events report changes; per frame a
	// pass hands over the view's planes or spheres and nothing else.
	class GpuScene {
	public:
		enum class List : uint8_t {
			GBuffer, // opaque surfaces, grouped by pipeline and textures
			Shadow,  // every caster, grouped by cutout and mask texture
			Outline, // entities with OutlineComponent, one group
			Glass,   // transparent surfaces, one batch, drawn sorted by sortGlass
			Count
		};

		struct DrawGroup {
			// GBuffer
			std::string vertexShader;
			std::string fragmentShader;
			ComponentsModule::Materials textures;
			uint64_t textureKey = 0;
			// Shadow
			TextureHandle maskTexture; // empty: no mask, bind the default
			bool cutout = false;       // needs the fragment shader: alpha cutout, mask or dithered opacity
			bool transparent = false;  // drawn as the glass shadow proxy; point lights skip it

			uint32_t firstBatch = 0;
			uint32_t batchCount = 0;
		};

		// The output of culling one list for one view, drawn from the same frame's command list.
		struct View {
			BufferHandle visible;   // SfeInstance records, bound at 3 by the draw
			BufferHandle commands;  // one DrawIndexedArgs per batch (glass: per sorted instance)
			BufferHandle keys;      // glass only: the sort keys
			size_t visibleCapacity = 0;
			size_t commandCapacity = 0;
			size_t keyCapacity = 0;
			// Per frame slot: the view parameters are written from the CPU while an earlier frame may still
			// be reading its own copy.
			std::array<BufferHandle, 2> params{};
			std::array<DescriptorSetHandle, 2> sets{};
			std::array<std::array<uint32_t, 8>, 2> boundIds{};
		};

		void init();
		// Any thread. Rebuilds every list from the registry on the next update.
		void markDirty() { mDirty.store(true, std::memory_order_release); }
		// Any thread. The entity's instances are read again from its components on the next update, or dropped
		// when it no longer has any.
		void markEntity(ecss::EntityId entity);
		// Transparent casters draw this mesh into the cascades instead of their own.
		void setGlassShadowProxy(const RenderMeshData& mesh) { mGlassShadowProxy = mesh; markDirty(); }
		// Main thread, under the world read lock, before the frame is recorded.
		void update(GpuMaterialTable& materials);

		// Record the culling dispatch into `view`. Outside a render pass. False when there is nothing to draw.
		bool cullFrustum(CommandListHandle cmd, List list, View& view, const FrustumModule::Frustum& frustum);
		// Visible when the bounds touch any sphere (xyz centre, w radius). At most kMaxSpheres.
		bool cullSpheres(CommandListHandle cmd, List list, View& view, const std::vector<Math::Vec4>& spheres);

		// Culls the glass list and sorts what is left far to near into `view`: command i draws the i-th farthest
		// instance, culled ones trail with no instances. Draw with drawSorted. Outside a render pass.
		bool sortGlass(CommandListHandle cmd, View& view, const FrustumModule::Frustum& frustum, const Math::Vec3& cameraPos, const Math::Vec3& viewDir);
		void drawSorted(CommandListHandle cmd, const View& view) const;

		const std::vector<DrawGroup>& groups(List list) const { return mLists[static_cast<size_t>(list)].groups; }
		// The largest entity id any instance has referred to: the culls read its transform.
		ecss::EntityId maxEntity() const { return mMaxEntity; }
		// The group's indirect call. Pipeline, descriptor sets and the shared vertex and index buffers are the
		// caller's.
		void draw(CommandListHandle cmd, const View& view, const DrawGroup& group) const;

		static constexpr uint32_t kMaxSpheres = 16;

		// Mirror the std430 structs in shaders/gpu_cull.cs and gpu_glass.cs.
		struct InstanceRecord {
			uint32_t entity = 0;
			uint32_t mesh = 0;
			uint32_t material = 0;
			uint32_t pad = 0;
		};
		struct BatchRecord {
			uint32_t firstInstance = 0;
			uint32_t instanceCount = 0;
			uint32_t mesh = 0;
			uint32_t pad = 0;
		};

	private:
		// Who owns an instance slot: the entity and the index of the slot's reference in mEntityRefs.
		struct Owner {
			ecss::EntityId entity = 0;
			uint32_t ref = 0;
		};
		struct BatchState {
			uint32_t capacity = 0;
			std::vector<Owner> owners; // one per live instance, in slot order
		};
		struct GroupState {
			uint32_t batchCapacity = 0;
			std::unordered_map<uint32_t, uint32_t> batchByKey; // mesh (glass: 0) -> batch index inside the group
		};
		struct InstanceRef {
			List list = List::GBuffer;
			uint32_t group = 0;
			uint32_t batch = 0; // inside the group
			uint32_t slot = 0;  // inside the batch
		};

		// Batches keep spare slots, and so do groups: an instance or a batch is appended in place, and only a full
		// one moves to the end with twice the room. What that leaves behind is reclaimed by a full rebuild.
		struct ListData {
			std::vector<DrawGroup> groups;
			BufferHandle instances;
			BufferHandle batches;
			uint32_t instanceCount = 0; // slots on the GPU, holes included
			uint32_t batchCount = 0;

			std::vector<GroupState> groupStates;
			std::unordered_map<std::string, uint32_t> groupByKey;
			std::vector<InstanceRecord> instanceMirror;
			std::vector<BatchRecord> batchMirror;
			std::vector<BatchState> batchStates;
			uint32_t liveInstances = 0;
			// Slots some batch or group still owns, spare room included; the rest were left behind by moves.
			size_t ownedInstances = 0;
			size_t ownedBatches = 0;
			size_t instanceGpuCapacity = 0;
			size_t batchGpuCapacity = 0;
			uint32_t instanceDirtyLo = UINT32_MAX;
			uint32_t instanceDirtyHi = 0;
			uint32_t batchDirtyLo = UINT32_MAX;
			uint32_t batchDirtyHi = 0;
		};

		void rebuildAll(GpuMaterialTable& materials);
		void rebuildOutline(GpuMaterialTable& materials);
		void addEntity(ecss::EntityId entity, GpuMaterialTable& materials);
		void removeEntity(ecss::EntityId entity);
		uint32_t groupFor(List list, const std::string& key, DrawGroup&& prototype);
		void addInstance(std::vector<InstanceRef>& refs, ecss::EntityId entity, List list, uint32_t group, uint32_t batchKey, const InstanceRecord& record);
		void removeInstance(const InstanceRef& ref);
		void growBatch(ListData& data, uint32_t batch, uint32_t capacity);
		void growGroup(ListData& data, uint32_t group, uint32_t capacity);
		void compact(ListData& data);
		void uploadList(ListData& data, const char* name);
		void uploadMeshes();
		// First slot and count of the one glass batch.
		std::pair<uint32_t, uint32_t> glassRange() const;
		bool cull(CommandListHandle cmd, List list, View& view, uint32_t mode, const float* shapes, uint32_t shapeCount);
		// Grows the view's output buffers to `instances` / `commands` entries and writes its parameters for this slot.
		uint32_t prepareView(View& view, uint32_t instances, uint32_t commands, uint32_t mode, const float* shapes, uint32_t shapeCount, uint32_t batchCount);

		std::atomic<bool> mDirty{ true };
		std::array<ListData, static_cast<size_t>(List::Count)> mLists;
		std::unordered_map<ecss::EntityId, std::vector<InstanceRef>> mEntityRefs;
		ecss::EntityId mMaxEntity = 0;
		std::mutex mPendingMutex;
		std::vector<ecss::EntityId> mPending;
		std::vector<ecss::EntityId> mOutlined;
		RenderMeshData mGlassShadowProxy;

		BufferHandle mMeshes;
		size_t mMeshesUploaded = 0;
		size_t mMeshCapacity = 0;

		DescriptorSetLayoutHandle mCullLayout;
		PipelineHandle mCullPipeline;
		DescriptorSetLayoutHandle mGlassLayout;
		PipelineHandle mGlassPipeline;
	};
}
