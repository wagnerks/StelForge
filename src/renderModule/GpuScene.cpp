#include "GpuScene.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "renderModule/GpuMaterialTable.h"
#include "renderModule/MaterialRules.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE::Render {
	namespace {
		// Mirror the std430 structs in shaders/gpu_cull.cs.
		struct SceneInstance {
			uint32_t entity = 0;
			uint32_t mesh = 0;
			uint32_t material = 0;
			uint32_t pad = 0;
		};
		struct Batch {
			uint32_t firstInstance = 0;
			uint32_t instanceCount = 0;
			uint32_t mesh = 0;
			uint32_t pad = 0;
		};
		struct CullPush {
			float planes[24]{};
			uint32_t batchCount = 0;
			uint32_t pad[3]{};
		};
		static_assert(sizeof(GpuMesh) == 48 && sizeof(SceneInstance) == 16 && sizeof(Batch) == 16);
		static_assert(sizeof(CullPush) == 112);

		constexpr uint32_t kLocalSize = 16;

		void retire(const BufferHandle buffer) {
			if (!buffer) {
				return;
			}
			if (auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>()) {
				renderSystem->retireBuffer(buffer);
			}
			else {
				Engine::instance()->rhi()->destroyBuffer(buffer);
			}
		}

		BufferHandle createStorage(const size_t size, const BufferUsage usage, const void* data, const char* name) {
			BufferDesc desc;
			desc.size = (std::max)(size, size_t{ 16 });
			desc.usage = usage;
			desc.cpuAccess = CpuAccess::None;
			desc.debugName = name;
			return Engine::instance()->rhi()->createBuffer(desc, data);
		}

		void setPlane(float* out, const FrustumModule::Plane& plane) {
			out[0] = plane.normal.x;
			out[1] = plane.normal.y;
			out[2] = plane.normal.z;
			out[3] = plane.distance;
		}
	}

	void GpuScene::init() {
		auto* rhi = Engine::instance()->rhi();
		DescriptorSetLayoutDesc layout;
		layout.bindings = {
			{ 1, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 26, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 27, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 28, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 29, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 30, DescriptorType::StorageBuffer, ShaderStage::Compute },
		};
		mCullLayout = rhi->createDescriptorSetLayout(layout);
		ComputePipelineDesc desc;
		desc.compute = rhi->loadShader(ShaderStage::Compute, "shaders/gpu_cull.cs");
		desc.descriptorSetLayouts = { mCullLayout };
		desc.pushConstantSize = sizeof(CullPush);
		mCullPipeline = rhi->createComputePipeline(desc);
	}

	void GpuScene::update(GpuMaterialTable& materials) {
		if (mDirty.exchange(false, std::memory_order_acq_rel)) {
			rebuild(materials);
		}
		// After the rebuild: every mesh an instance refers to already exists by then.
		uploadMeshes();
	}

	void GpuScene::uploadMeshes() {
		auto* registry = MeshVaoRegistry::instance();
		if (registry->meshCount() == mMeshesUploaded) {
			return;
		}
		const auto table = registry->meshTable();
		auto* rhi = Engine::instance()->rhi();
		if (!mMeshes || table.size() > mMeshCapacity) {
			retire(mMeshes);
			mMeshCapacity = (std::max)(mMeshCapacity, size_t{ 64 });
			while (mMeshCapacity < table.size()) {
				mMeshCapacity *= 2;
			}
			mMeshes = createStorage(mMeshCapacity * sizeof(GpuMesh), BufferUsage::Storage, nullptr, "GpuScene meshes");
		}
		rhi->updateBuffer(mMeshes, 0, table.size() * sizeof(GpuMesh), table.data());
		mMeshesUploaded = table.size();
	}

	void GpuScene::rebuild(GpuMaterialTable& materials) {
		struct Item {
			uint32_t group = 0;
			SceneInstance instance;
		};
		static const ComponentsModule::MaterialComponent kNoMaterial;

		std::vector<Item> items;
		std::vector<DrawGroup> groups;
		std::unordered_map<std::string, uint32_t> groupByKey;

		auto& registry = ECSHandler::registry();
		for (auto [entity, transform, meshComp, matComp] : registry.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>()) {
			if (!transform || !meshComp) {
				continue;
			}
			meshComp->forEachDrawMesh(matComp, [&](const RenderMeshData& mesh, const MaterialComponent* mat) {
				if ((mat && mat->transparent) || mesh.indexCount == 0) {
					return;
				}
				const auto& material = mat ? *mat : kNoMaterial;
				const auto& vs = material.vertexShader.empty() ? std::string(ComponentsModule::DefaultGBufferVS) : material.vertexShader;
				const auto& fs = material.fragmentShader.empty() ? std::string(ComponentsModule::DefaultGBufferFS) : material.fragmentShader;
				const auto textureKey = MaterialRules::textureKey(material.materials);
				const auto key = vs + '|' + fs + '|' + std::to_string(textureKey);
				const auto [it, inserted] = groupByKey.try_emplace(key, static_cast<uint32_t>(groups.size()));
				if (inserted) {
					groups.push_back({ vs, fs, material.materials, textureKey });
				}
				items.push_back({ it->second, { entity, mesh.meshId, materials.indexFor(material), 0 } });
			});
		}

		// Group, then mesh: each group becomes one contiguous run of batches. Stable, so the order inside a
		// batch follows the registry and a rebuild of the same scene gives the same buffers.
		std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
			return a.group != b.group ? a.group < b.group : a.instance.mesh < b.instance.mesh;
		});

		std::vector<SceneInstance> instances;
		std::vector<Batch> batches;
		instances.reserve(items.size());
		for (size_t i = 0; i < items.size(); ++i) {
			const auto& item = items[i];
			const bool newGroup = i == 0 || item.group != items[i - 1].group;
			if (newGroup) {
				groups[item.group].firstBatch = static_cast<uint32_t>(batches.size());
			}
			if (newGroup || item.instance.mesh != items[i - 1].instance.mesh) {
				batches.push_back({ static_cast<uint32_t>(i), 0, item.instance.mesh, 0 });
				++groups[item.group].batchCount;
			}
			++batches.back().instanceCount;
			instances.push_back(item.instance);
		}

		retire(mInstances);
		retire(mBatches);
		mInstances = createStorage(instances.size() * sizeof(SceneInstance), BufferUsage::Storage, instances.empty() ? nullptr : instances.data(), "GpuScene instances");
		mBatches = createStorage(batches.size() * sizeof(Batch), BufferUsage::Storage, batches.empty() ? nullptr : batches.data(), "GpuScene batches");
		mInstanceCount = static_cast<uint32_t>(instances.size());
		mBatchCount = static_cast<uint32_t>(batches.size());
		mGroups = std::move(groups);
	}

	bool GpuScene::cull(const CommandListHandle cmd, View& view, const FrustumModule::Frustum& frustum, const BufferHandle transforms) {
		if (mBatchCount == 0 || !mCullPipeline || !mMeshes || !transforms) {
			return false;
		}
		auto* rhi = Engine::instance()->rhi();

		if (!view.visible || view.visibleCapacity < mInstanceCount) {
			retire(view.visible);
			view.visibleCapacity = mInstanceCount + mInstanceCount / 2;
			view.visible = createStorage(view.visibleCapacity * 2 * sizeof(uint32_t), BufferUsage::Storage, nullptr, "GpuScene visible");
		}
		if (!view.commands || view.commandCapacity < mBatchCount) {
			retire(view.commands);
			view.commandCapacity = mBatchCount + mBatchCount / 2;
			view.commands = createStorage(view.commandCapacity * kCommandStride, BufferUsage::Storage | BufferUsage::Indirect, nullptr, "GpuScene commands");
		}
		if (!view.set) {
			view.set = rhi->allocateDescriptorSet(mCullLayout);
		}

		// Rewritten only when a buffer was replaced: a write to a set that is already bound costs a copy.
		const uint32_t ids[6] = { transforms.id, mMeshes.id, mInstances.id, mBatches.id, view.visible.id, view.commands.id };
		if (!std::equal(std::begin(ids), std::end(ids), std::begin(view.boundIds))) {
			std::copy(std::begin(ids), std::end(ids), std::begin(view.boundIds));
			rhi->updateDescriptorSets({
				{ .set = view.set, .binding = 1, .buffers{ { transforms } } },
				{ .set = view.set, .binding = 26, .buffers{ { mMeshes } } },
				{ .set = view.set, .binding = 27, .buffers{ { mInstances } } },
				{ .set = view.set, .binding = 28, .buffers{ { mBatches } } },
				{ .set = view.set, .binding = 29, .buffers{ { view.visible } } },
				{ .set = view.set, .binding = 30, .buffers{ { view.commands } } },
			});
		}

		CullPush push;
		setPlane(push.planes + 0, frustum.leftFace);
		setPlane(push.planes + 4, frustum.rightFace);
		setPlane(push.planes + 8, frustum.topFace);
		setPlane(push.planes + 12, frustum.bottomFace);
		setPlane(push.planes + 16, frustum.nearFace);
		setPlane(push.planes + 20, frustum.farFace);
		push.batchCount = mBatchCount;

		rhi->cmdBindPipeline(cmd, mCullPipeline);
		rhi->cmdBindDescriptorSets(cmd, mCullPipeline, 0, &view.set, 1);
		rhi->cmdPushConstants(cmd, mCullPipeline, ShaderStage::Compute, &push, sizeof(push));
		rhi->cmdDispatch(cmd, (mBatchCount + kLocalSize - 1) / kLocalSize, 1, 1);
		rhi->cmdPipelineBarrier(cmd, {
			PipelineStage::Compute,
			PipelineStage::DrawIndirect | PipelineStage::VertexShader,
			AccessMask::ShaderWrite,
			AccessMask::IndirectCommandRead | AccessMask::ShaderRead,
		});
		return true;
	}
}
