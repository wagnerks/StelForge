#pragma once

#include <array>
#include <atomic>
#include <string>
#include <vector>

#include "assetsModule/RenderMeshData.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "componentsModule/MaterialComponent.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render {
	class GpuMaterialTable;

	// What the GPU needs to cull and draw the scene without the CPU touching it per frame: every drawable
	// submesh as an instance, sorted so that one draw group is one indirect call and one batch (a mesh
	// inside a group) is one command in it. A list is rebuilt only when its set of instances changes; per
	// frame a pass hands over the view's planes or spheres and nothing else.
	class GpuScene {
	public:
		enum class List : uint8_t {
			GBuffer, // opaque surfaces, grouped by pipeline and textures
			Shadow,  // every caster, grouped by cutout and mask texture
			Outline, // entities with OutlineComponent, one group
			Glass,   // transparent surfaces, one group, drawn sorted by sortGlass
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
		// Any thread.
		void markDirty() { mDirty.store(true, std::memory_order_release); }
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
		// The group's indirect call. Pipeline, descriptor sets and the shared vertex and index buffers are the
		// caller's.
		void draw(CommandListHandle cmd, const View& view, const DrawGroup& group) const;

		static constexpr uint32_t kMaxSpheres = 16;

	private:
		struct ListData {
			std::vector<DrawGroup> groups;
			BufferHandle instances;
			BufferHandle batches;
			uint32_t instanceCount = 0;
			uint32_t batchCount = 0;
		};

		void rebuild(GpuMaterialTable& materials);
		void rebuildOutline(GpuMaterialTable& materials);
		void uploadMeshes();
		bool cull(CommandListHandle cmd, List list, View& view, uint32_t mode, const float* shapes, uint32_t shapeCount);
		// Grows the view's output buffers to `instances` / `commands` entries and writes its parameters for this slot.
		uint32_t prepareView(View& view, uint32_t instances, uint32_t commands, uint32_t mode, const float* shapes, uint32_t shapeCount, uint32_t batchCount);

		std::atomic<bool> mDirty{ true };
		std::array<ListData, static_cast<size_t>(List::Count)> mLists;
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
