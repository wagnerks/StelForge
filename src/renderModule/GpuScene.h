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
			// Two per slot: with occlusion each phase writes its own before either runs.
			std::vector<BufferHandle> params;
			std::vector<DescriptorSetHandle> sets;
			std::vector<std::array<uint32_t, 10>> boundIds;

			// Occlusion (cullVisibleLastFrame / cullOccluded)
			BufferHandle lastVisible; // per instance slot, kept across frames
			size_t lastVisibleCapacity = 0;
			uint32_t secondPhaseOffset = 0; // the second phase's commands follow the first's
			BufferHandle pyramid;
			uint32_t depthWidth = 0;
			uint32_t depthHeight = 0;
			uint32_t levels = 0;
			std::array<uint32_t, 16> levelOffsets{};
			std::vector<DescriptorSetHandle> pyramidSets;
			std::vector<std::array<uint32_t, 2>> pyramidBoundIds;
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

		// Occlusion culling in two phases, for a view that renders its own depth. Phase one keeps what passed the
		// occlusion test last frame; draw it with draw(..., false). Then build the depth pyramid from that depth,
		// and phase two tests everything else against it; draw that with draw(..., true) into the same targets.
		// Nothing visible is lost: an instance hidden last frame is tested against this frame's depth.
		// Outside a render pass, like cullFrustum.
		bool cullVisibleLastFrame(CommandListHandle cmd, List list, View& view, const FrustumModule::Frustum& frustum);
		void buildDepthPyramid(CommandListHandle cmd, View& view, TextureHandle depth, SamplerHandle sampler, uint32_t width, uint32_t height);
		bool cullOccluded(CommandListHandle cmd, List list, View& view, const FrustumModule::Frustum& frustum, const Math::Mat4& viewProj);

		// Culls the glass list and sorts what is left far to near into `view`: command i draws the i-th farthest
		// instance, culled ones trail with no instances. Draw with drawSorted. Outside a render pass.
		bool sortGlass(CommandListHandle cmd, View& view, const FrustumModule::Frustum& frustum, const Math::Vec3& cameraPos, const Math::Vec3& viewDir);
		void drawSorted(CommandListHandle cmd, const View& view) const;

		const std::vector<DrawGroup>& groups(List list) const { return mLists[static_cast<size_t>(list)].groups; }
		// The largest entity id any instance has referred to: the culls read its transform.
		ecss::EntityId maxEntity() const { return mMaxEntity; }

		// For tests: every entity's references lead to instances it owns, and the ownership counters match the
		// layout. Between updates only.
		bool validate() const;
		uint64_t compactions() const { return mCompactions; }
		// For tests: keep a copy of every upload, so validate() also checks the GPU buffers hold the layout.
		// For tests: batches holding instances of `mesh`, and the most instances in one batch.
		size_t batchesOfMesh(List list, uint32_t mesh) const;
		uint32_t largestBatch(List list) const;
		void trackUploads(bool track) {
			mTrackUploads = track;
			for (auto& data : mLists) {
				data.instanceDirty.all();
				data.batchDirty.all();
			}
		}
		// The group's indirect call. Pipeline, descriptor sets and the shared vertex and index buffers are the
		// caller's.
		void draw(CommandListHandle cmd, const View& view, const DrawGroup& group, bool secondPhase = false) const;

		static constexpr uint32_t kMaxSpheres = 16;
		// Instances per batch before another batch of the same mesh is started. Glass stays one batch: it is sorted as one.
		static constexpr uint32_t kMaxBatchInstances = 1024;

		// Mirror the std430 structs in shaders/gpu_cull.cs and gpu_glass.cs.
		struct InstanceRecord {
			uint32_t entity = 0;
			uint32_t mesh = 0;
			uint32_t material = 0;
			uint32_t boneOffset = 0; // RenderSystem::boneRange
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
			// mesh (glass: 0) -> its batches inside the group. A mesh with many instances is split into several, so
			// the cull, one invocation per batch, still runs in parallel.
			std::unordered_map<uint32_t, std::vector<uint32_t>> batchesByKey;
		};
		struct InstanceRef {
			List list = List::GBuffer;
			uint32_t group = 0;
			uint32_t batch = 0; // inside the group
			uint32_t slot = 0;  // inside the batch
		};

		// Slot ranges to upload. A few separate ranges, so an edit near the start and a batch growing at the end do
		// not upload everything between them; ranges close together are merged to keep the list short.
		struct DirtyRanges {
			std::vector<std::pair<uint32_t, uint32_t>> ranges; // [lo, hi), sorted, disjoint
			void add(uint32_t lo, uint32_t hi);
			void all() { ranges.assign(1, { 0, UINT32_MAX }); }
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
			DirtyRanges instanceDirty;
			DirtyRanges batchDirty;
			// What was uploaded, when tracking.
			std::vector<InstanceRecord> uploadedInstances;
			std::vector<BatchRecord> uploadedBatches;
		};

		void rebuildAll(GpuMaterialTable& materials);
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
		struct CullParams;
		CullParams frustumParams(const FrustumModule::Frustum& frustum) const;
		bool cull(CommandListHandle cmd, List list, View& view, CullParams& params);
		// Grows the view's output buffers to `instances` / `commands` entries and writes its parameters. Returns the
		// index of this frame's (and phase's) params and set.
		uint32_t prepareView(View& view, uint32_t instances, uint32_t commands, const CullParams& params);

		std::atomic<bool> mDirty{ true };
		std::array<ListData, static_cast<size_t>(List::Count)> mLists;
		std::unordered_map<ecss::EntityId, std::vector<InstanceRef>> mEntityRefs;
		ecss::EntityId mMaxEntity = 0;
		uint64_t mCompactions = 0;
		// The table instances hold references in, for removals; set by update.
		GpuMaterialTable* mMaterials = nullptr;
		bool mTrackUploads = false;
		std::mutex mPendingMutex;
		std::vector<ecss::EntityId> mPending;
		std::vector<ecss::EntityId> mOutlined;
		RenderMeshData mGlassShadowProxy;

		BufferHandle mMeshes;
		size_t mMeshesUploaded = 0;
		size_t mMeshCapacity = 0;

		DescriptorSetLayoutHandle mCullLayout;
		PipelineHandle mCullPipeline;
		DescriptorSetLayoutHandle mHizLayout;
		PipelineHandle mHizPipeline;
		BufferHandle mEmptyBuffer; // bound where a cull without occlusion has no buffer
		DescriptorSetLayoutHandle mGlassLayout;
		PipelineHandle mGlassPipeline;
	};
}
