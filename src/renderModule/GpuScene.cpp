#include "GpuScene.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/OutlineComponent.h"
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
		struct CullParams {
			uint32_t mode = 0; // 0: planes, 1: spheres
			uint32_t shapeCount = 0;
			uint32_t batchCount = 0;
			uint32_t pad = 0;
			float shapes[GpuScene::kMaxSpheres * 4]{};
		};
		static_assert(sizeof(GpuMesh) == 48 && sizeof(SceneInstance) == 16 && sizeof(Batch) == 16);
		static_assert(sizeof(CullParams) == 16 + GpuScene::kMaxSpheres * 16);

		constexpr uint32_t kLocalSize = 16;
		constexpr uint32_t kGlassLocalSize = 64;
		constexpr uint32_t kCommandStride = 20;

		struct Item {
			uint32_t group = 0;
			SceneInstance instance;
		};

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

		BufferHandle createBuffer(const size_t size, const BufferUsage usage, const CpuAccess access, const void* data, const char* name) {
			BufferDesc desc;
			desc.size = (std::max)(size, size_t{ 16 });
			desc.usage = usage;
			desc.cpuAccess = access;
			desc.debugName = name;
			return Engine::instance()->rhi()->createBuffer(desc, data);
		}

		uint32_t frameSlots() {
			return (std::max)(1u, (std::min)(2u, Engine::instance()->rhi()->framesInFlight()));
		}

		// Group, then mesh: each group becomes one contiguous run of batches. Stable, so the order inside a
		// batch follows the registry and a rebuild of the same scene gives the same buffers.
		void pack(std::vector<Item>& items, std::vector<GpuScene::DrawGroup>& groups, BufferHandle& instancesBuffer,
			BufferHandle& batchesBuffer, uint32_t& instanceCount, uint32_t& batchCount, const char* name)
		{
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

			retire(instancesBuffer);
			retire(batchesBuffer);
			instancesBuffer = createBuffer(instances.size() * sizeof(SceneInstance), BufferUsage::Storage, CpuAccess::None, instances.empty() ? nullptr : instances.data(), name);
			batchesBuffer = createBuffer(batches.size() * sizeof(Batch), BufferUsage::Storage, CpuAccess::None, batches.empty() ? nullptr : batches.data(), name);
			instanceCount = static_cast<uint32_t>(instances.size());
			batchCount = static_cast<uint32_t>(batches.size());
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
			{ 31, DescriptorType::StorageBuffer, ShaderStage::Compute },
		};
		mCullLayout = rhi->createDescriptorSetLayout(layout);
		ComputePipelineDesc desc;
		desc.compute = rhi->loadShader(ShaderStage::Compute, "shaders/gpu_cull.cs");
		desc.descriptorSetLayouts = { mCullLayout };
		mCullPipeline = rhi->createComputePipeline(desc);

		DescriptorSetLayoutDesc glassLayout;
		glassLayout.bindings = {
			{ 1, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 26, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 27, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 29, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 30, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 31, DescriptorType::StorageBuffer, ShaderStage::Compute },
			{ 32, DescriptorType::StorageBuffer, ShaderStage::Compute },
		};
		mGlassLayout = rhi->createDescriptorSetLayout(glassLayout);
		ComputePipelineDesc glassDesc;
		glassDesc.compute = rhi->loadShader(ShaderStage::Compute, "shaders/gpu_glass.cs");
		glassDesc.descriptorSetLayouts = { mGlassLayout };
		glassDesc.pushConstantSize = 20;
		mGlassPipeline = rhi->createComputePipeline(glassDesc);
	}

	void GpuScene::update(GpuMaterialTable& materials) {
		const bool dirty = mDirty.exchange(false, std::memory_order_acq_rel);
		if (dirty) {
			rebuild(materials);
		}

		// OutlineComponent is added and removed straight on the registry, with no event, so the outlined set
		// is compared every frame. It is a handful of entities.
		std::vector<ecss::EntityId> outlined;
		for (auto [entity, outline] : ECSHandler::registry().view<const OutlineComponent>()) {
			outlined.push_back(entity);
		}
		if (dirty || outlined != mOutlined) {
			mOutlined = std::move(outlined);
			rebuildOutline(materials);
		}

		// After the rebuilds: every mesh an instance refers to already exists by then.
		uploadMeshes();
	}

	void GpuScene::uploadMeshes() {
		auto* registry = MeshVaoRegistry::instance();
		if (registry->meshCount() == mMeshesUploaded) {
			return;
		}
		const auto table = registry->meshTable();
		if (!mMeshes || table.size() > mMeshCapacity) {
			retire(mMeshes);
			mMeshCapacity = (std::max)(mMeshCapacity, size_t{ 64 });
			while (mMeshCapacity < table.size()) {
				mMeshCapacity *= 2;
			}
			mMeshes = createBuffer(mMeshCapacity * sizeof(GpuMesh), BufferUsage::Storage, CpuAccess::None, nullptr, "GpuScene meshes");
		}
		Engine::instance()->rhi()->updateBuffer(mMeshes, 0, table.size() * sizeof(GpuMesh), table.data());
		mMeshesUploaded = table.size();
	}

	void GpuScene::rebuild(GpuMaterialTable& materials) {
		static const ComponentsModule::MaterialComponent kNoMaterial;

		std::vector<Item> gbufferItems;
		std::vector<DrawGroup> gbufferGroups;
		std::unordered_map<std::string, uint32_t> gbufferByKey;

		std::vector<Item> shadowItems;
		std::vector<DrawGroup> shadowGroups;
		std::unordered_map<uint64_t, uint32_t> shadowByKey;

		std::vector<Item> glassItems;

		auto& registry = ECSHandler::registry();
		for (auto [entity, transform, meshComp, matComp] : registry.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>()) {
			if (!transform || !meshComp) {
				continue;
			}
			meshComp->forEachDrawMesh(matComp, [&](const RenderMeshData& mesh, const MaterialComponent* mat) {
				const auto& material = mat ? *mat : kNoMaterial;
				const bool transparent = mat && mat->transparent;
				const uint32_t materialIndex = materials.indexFor(material);

				// Shadow casters: a transparent one casts through the glass proxy when there is one.
				const auto& caster = transparent && mGlassShadowProxy ? mGlassShadowProxy : mesh;
				if (caster.indexCount != 0) {
					const bool dithered = material.opacity > 0.f && material.opacity < 0.999f;
					const float cutoff = MaterialRules::cutoutCutoff(material.alphaCutoff, material.alphaFromMaps, material.fragmentShader);
					const bool cutout = material.alphaFromMaps || cutoff > 0.f || dithered;
					const auto mask = !cutout ? TextureHandle{}
						: material.alphaFromMaps ? MaterialRules::maskTexture(material.materials) : MaterialRules::diffuseTexture(material.materials);
					const uint64_t key = (static_cast<uint64_t>(mask.id) << 2) | (cutout ? 1u : 0u) | (transparent ? 2u : 0u);
					const auto [it, inserted] = shadowByKey.try_emplace(key, static_cast<uint32_t>(shadowGroups.size()));
					if (inserted) {
						DrawGroup group;
						group.maskTexture = mask;
						group.cutout = cutout;
						group.transparent = transparent;
						shadowGroups.push_back(std::move(group));
					}
					shadowItems.push_back({ it->second, { entity, caster.meshId, materialIndex, 0 } });
				}

				if (mesh.indexCount == 0) {
					return;
				}
				if (transparent) {
					glassItems.push_back({ 0, { entity, mesh.meshId, materialIndex, 0 } });
					return;
				}
				const auto vs = material.vertexShader.empty() ? std::string(ComponentsModule::DefaultGBufferVS) : material.vertexShader;
				const auto fs = material.fragmentShader.empty() ? std::string(ComponentsModule::DefaultGBufferFS) : material.fragmentShader;
				const auto textureKey = MaterialRules::textureKey(material.materials);
				const auto [it, inserted] = gbufferByKey.try_emplace(vs + '|' + fs + '|' + std::to_string(textureKey), static_cast<uint32_t>(gbufferGroups.size()));
				if (inserted) {
					DrawGroup group;
					group.vertexShader = vs;
					group.fragmentShader = fs;
					group.textures = material.materials;
					group.textureKey = textureKey;
					gbufferGroups.push_back(std::move(group));
				}
				gbufferItems.push_back({ it->second, { entity, mesh.meshId, materialIndex, 0 } });
			});
		}

		auto& gbuffer = mLists[static_cast<size_t>(List::GBuffer)];
		pack(gbufferItems, gbufferGroups, gbuffer.instances, gbuffer.batches, gbuffer.instanceCount, gbuffer.batchCount, "GpuScene gbuffer");
		gbuffer.groups = std::move(gbufferGroups);

		auto& shadow = mLists[static_cast<size_t>(List::Shadow)];
		pack(shadowItems, shadowGroups, shadow.instances, shadow.batches, shadow.instanceCount, shadow.batchCount, "GpuScene shadow");
		shadow.groups = std::move(shadowGroups);

		// Glass keeps registry order here; sortGlass orders it by depth every frame.
		std::vector<DrawGroup> glassGroups(1);
		auto& glass = mLists[static_cast<size_t>(List::Glass)];
		pack(glassItems, glassGroups, glass.instances, glass.batches, glass.instanceCount, glass.batchCount, "GpuScene glass");		glass.groups = std::move(glassGroups);
	}

	void GpuScene::rebuildOutline(GpuMaterialTable& materials) {
		static const ComponentsModule::MaterialComponent kNoMaterial;
		const uint32_t materialIndex = materials.indexFor(kNoMaterial);

		std::vector<Item> items;
		std::vector<DrawGroup> groups(1);
		auto& registry = ECSHandler::registry();
		for (const auto entity : mOutlined) {
			const auto meshComp = registry.pinComponent<const MeshComponent>(entity);
			if (!meshComp || !registry.hasComponent<ComponentsModule::TransformMatComp>(entity)) {
				continue;
			}
			for (const auto& node : meshComp->meshGraph) {
				if (node.value && node.value.indexCount != 0) {
					items.push_back({ 0, { entity, node.value.meshId, materialIndex, 0 } });
				}
			}
		}

		auto& outline = mLists[static_cast<size_t>(List::Outline)];
		pack(items, groups, outline.instances, outline.batches, outline.instanceCount, outline.batchCount, "GpuScene outline");
		outline.groups = std::move(groups);
	}

	bool GpuScene::cullFrustum(const CommandListHandle cmd, const List list, View& view, const FrustumModule::Frustum& frustum) {
		float planes[24];
		setPlane(planes + 0, frustum.leftFace);
		setPlane(planes + 4, frustum.rightFace);
		setPlane(planes + 8, frustum.topFace);
		setPlane(planes + 12, frustum.bottomFace);
		setPlane(planes + 16, frustum.nearFace);
		setPlane(planes + 20, frustum.farFace);
		return cull(cmd, list, view, 0, planes, 6);
	}

	bool GpuScene::cullSpheres(const CommandListHandle cmd, const List list, View& view, const std::vector<Math::Vec4>& spheres) {
		const auto count = static_cast<uint32_t>((std::min)(spheres.size(), size_t{ kMaxSpheres }));
		std::vector<float> shapes(count * 4);
		for (uint32_t i = 0; i < count; ++i) {
			for (int c = 0; c < 4; ++c) {
				shapes[i * 4 + c] = spheres[i][c];
			}
		}
		return count != 0 && cull(cmd, list, view, 1, shapes.data(), count);
	}

	uint32_t GpuScene::prepareView(View& view, const uint32_t instances, const uint32_t commands, const uint32_t mode, const float* shapes, const uint32_t shapeCount, const uint32_t batchCount) {
		auto* rhi = Engine::instance()->rhi();
		if (!view.visible || view.visibleCapacity < instances) {
			retire(view.visible);
			view.visibleCapacity = instances + instances / 2;
			view.visible = createBuffer(view.visibleCapacity * 2 * sizeof(uint32_t), BufferUsage::Storage, CpuAccess::None, nullptr, "GpuScene visible");
		}
		if (!view.commands || view.commandCapacity < commands) {
			retire(view.commands);
			view.commandCapacity = commands + commands / 2;
			view.commands = createBuffer(view.commandCapacity * kCommandStride, BufferUsage::Storage | BufferUsage::Indirect, CpuAccess::None, nullptr, "GpuScene commands");
		}

		const uint32_t slot = rhi->frameInFlight() % frameSlots();
		if (!view.params[slot]) {
			view.params[slot] = createBuffer(sizeof(CullParams), BufferUsage::Storage, CpuAccess::Write, nullptr, "GpuScene view");
		}
		CullParams params;
		params.mode = mode;
		params.shapeCount = shapeCount;
		params.batchCount = batchCount;
		std::memcpy(params.shapes, shapes, shapeCount * 4 * sizeof(float));
		rhi->updateBuffer(view.params[slot], 0, sizeof(params), &params);
		return slot;
	}

	bool GpuScene::cull(const CommandListHandle cmd, const List list, View& view, const uint32_t mode, const float* shapes, const uint32_t shapeCount) {
		const auto& data = mLists[static_cast<size_t>(list)];
		auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>();
		const auto transforms = renderSystem ? renderSystem->transformsBuffer() : BufferHandle{};
		if (data.batchCount == 0 || !mCullPipeline || !mMeshes || !transforms) {
			return false;
		}
		auto* rhi = Engine::instance()->rhi();
		const uint32_t slot = prepareView(view, data.instanceCount, data.batchCount, mode, shapes, shapeCount, data.batchCount);
		if (!view.sets[slot]) {
			view.sets[slot] = rhi->allocateDescriptorSet(mCullLayout);
		}

		// Rewritten only when a buffer was replaced: a write to a set that is already bound costs a copy.
		const std::array<uint32_t, 8> ids = { transforms.id, mMeshes.id, data.instances.id, data.batches.id, view.visible.id, view.commands.id, view.params[slot].id, 0 };
		if (ids != view.boundIds[slot]) {
			view.boundIds[slot] = ids;
			const auto set = view.sets[slot];
			rhi->updateDescriptorSets({
				{ .set = set, .binding = 1, .buffers{ { transforms } } },
				{ .set = set, .binding = 26, .buffers{ { mMeshes } } },
				{ .set = set, .binding = 27, .buffers{ { data.instances } } },
				{ .set = set, .binding = 28, .buffers{ { data.batches } } },
				{ .set = set, .binding = 29, .buffers{ { view.visible } } },
				{ .set = set, .binding = 30, .buffers{ { view.commands } } },
				{ .set = set, .binding = 31, .buffers{ { view.params[slot] } } },
			});
		}

		rhi->cmdBindPipeline(cmd, mCullPipeline);
		rhi->cmdBindDescriptorSets(cmd, mCullPipeline, 0, &view.sets[slot], 1);
		rhi->cmdDispatch(cmd, (data.batchCount + kLocalSize - 1) / kLocalSize, 1, 1);
		rhi->cmdPipelineBarrier(cmd, {
			PipelineStage::Compute,
			PipelineStage::DrawIndirect | PipelineStage::VertexShader,
			AccessMask::ShaderWrite,
			AccessMask::IndirectCommandRead | AccessMask::ShaderRead,
		});
		return true;
	}

	bool GpuScene::sortGlass(const CommandListHandle cmd, View& view, const FrustumModule::Frustum& frustum, const Math::Vec3& cameraPos, const Math::Vec3& viewDir) {
		const auto& data = mLists[static_cast<size_t>(List::Glass)];
		auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>();
		const auto transforms = renderSystem ? renderSystem->transformsBuffer() : BufferHandle{};
		if (data.instanceCount == 0 || !mGlassPipeline || !mMeshes || !transforms) {
			return false;
		}
		auto* rhi = Engine::instance()->rhi();

		uint32_t padded = 1;
		while (padded < data.instanceCount) {
			padded <<= 1;
		}
		if (!view.keys || view.keyCapacity < padded) {
			retire(view.keys);
			view.keyCapacity = padded;
			view.keys = createBuffer(padded * 2 * sizeof(uint32_t), BufferUsage::Storage, CpuAccess::None, nullptr, "GpuScene glass keys");
		}

		float shapes[32]{};
		setPlane(shapes + 0, frustum.leftFace);
		setPlane(shapes + 4, frustum.rightFace);
		setPlane(shapes + 8, frustum.topFace);
		setPlane(shapes + 12, frustum.bottomFace);
		setPlane(shapes + 16, frustum.nearFace);
		setPlane(shapes + 20, frustum.farFace);
		for (int i = 0; i < 3; ++i) {
			shapes[24 + i] = cameraPos[i];
			shapes[28 + i] = viewDir[i];
		}
		const uint32_t slot = prepareView(view, data.instanceCount, data.instanceCount, 0, shapes, 8, 0);
		if (!view.sets[slot]) {
			view.sets[slot] = rhi->allocateDescriptorSet(mGlassLayout);
		}

		const std::array<uint32_t, 8> ids = { transforms.id, mMeshes.id, data.instances.id, view.keys.id, view.visible.id, view.commands.id, view.params[slot].id, 1 };
		if (ids != view.boundIds[slot]) {
			view.boundIds[slot] = ids;
			const auto set = view.sets[slot];
			rhi->updateDescriptorSets({
				{ .set = set, .binding = 1, .buffers{ { transforms } } },
				{ .set = set, .binding = 26, .buffers{ { mMeshes } } },
				{ .set = set, .binding = 27, .buffers{ { data.instances } } },
				{ .set = set, .binding = 29, .buffers{ { view.visible } } },
				{ .set = set, .binding = 30, .buffers{ { view.commands } } },
				{ .set = set, .binding = 31, .buffers{ { view.params[slot] } } },
				{ .set = set, .binding = 32, .buffers{ { view.keys } } },
			});
		}

		struct GlassStep {
			uint32_t stage = 0;
			uint32_t count = 0;
			uint32_t paddedCount = 0;
			uint32_t j = 0;
			uint32_t k = 0;
		} step;
		step.count = data.instanceCount;
		step.paddedCount = padded;

		rhi->cmdBindPipeline(cmd, mGlassPipeline);
		rhi->cmdBindDescriptorSets(cmd, mGlassPipeline, 0, &view.sets[slot], 1);
		const auto dispatch = [&](const uint32_t invocations) {
			rhi->cmdPushConstants(cmd, mGlassPipeline, ShaderStage::Compute, &step, sizeof(step));
			rhi->cmdDispatch(cmd, (invocations + kGlassLocalSize - 1) / kGlassLocalSize, 1, 1);
			// Each stage reads what the previous one wrote.
			rhi->cmdPipelineBarrier(cmd, { PipelineStage::Compute, PipelineStage::Compute, AccessMask::ShaderWrite, AccessMask::ShaderRead });
		};

		dispatch(padded);
		step.stage = 1;
		for (step.k = 2; step.k <= padded; step.k <<= 1) {
			for (step.j = step.k >> 1; step.j > 0; step.j >>= 1) {
				dispatch(padded);
			}
		}
		step.stage = 2;
		dispatch(data.instanceCount);

		rhi->cmdPipelineBarrier(cmd, {
			PipelineStage::Compute,
			PipelineStage::DrawIndirect | PipelineStage::VertexShader,
			AccessMask::ShaderWrite,
			AccessMask::IndirectCommandRead | AccessMask::ShaderRead,
		});
		return true;
	}

	void GpuScene::drawSorted(const CommandListHandle cmd, const View& view) const {
		const auto count = mLists[static_cast<size_t>(List::Glass)].instanceCount;
		if (count == 0) {
			return;
		}
		Engine::instance()->rhi()->cmdDrawIndexedIndirect(cmd, view.commands, 0, count, kCommandStride);
		SystemsModule::RenderDebugInfo::instance()->drawCalls++;
	}

	void GpuScene::draw(const CommandListHandle cmd, const View& view, const DrawGroup& group) const {
		if (group.batchCount == 0) {
			return;
		}
		Engine::instance()->rhi()->cmdDrawIndexedIndirect(cmd, view.commands, group.firstBatch * kCommandStride, group.batchCount, kCommandStride);
		SystemsModule::RenderDebugInfo::instance()->drawCalls++;
	}
}
