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
		using SceneInstance = GpuScene::InstanceRecord;
		using Batch = GpuScene::BatchRecord;
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
			Engine::instance()->rhi()->destroyBuffer(buffer);
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
			return (std::max)(1u, Engine::instance()->rhi()->framesInFlight());
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
		glassDesc.pushConstantSize = 24;
		mGlassPipeline = rhi->createComputePipeline(glassDesc);
	}

	void GpuScene::update(GpuMaterialTable& materials) {
		std::vector<ecss::EntityId> pending;
		{
			std::lock_guard lock(mPendingMutex);
			pending.swap(mPending);
		}

		bool rebuilt = mDirty.exchange(false, std::memory_order_acq_rel);
		if (!rebuilt) {
			std::sort(pending.begin(), pending.end());
			pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
			// Remove, then read again: a destroyed entity has nothing left to add, and a reused id adds its new self.
			for (const auto entity : pending) {
				removeEntity(entity);
				addEntity(entity, materials);
			}
			// Moves leave slots behind; once those outnumber the live ones the list is packed again. Spare room
			// inside a batch or group is not left behind, or compact's own spare would trigger it every frame.
			for (const auto list : { List::GBuffer, List::Shadow, List::Glass }) {
				auto& data = mLists[static_cast<size_t>(list)];
				const size_t leftInstances = data.instanceMirror.size() - data.ownedInstances;
				const size_t leftBatches = data.batchMirror.size() - data.ownedBatches;
				if (leftInstances > (std::max)(size_t{ 4096 }, static_cast<size_t>(data.liveInstances))
					|| leftBatches > (std::max)(size_t{ 1024 }, data.ownedBatches)) {
					compact(data);
				}
			}
		}
		if (rebuilt) {
			rebuildAll(materials);
		}
		uploadList(mLists[static_cast<size_t>(List::GBuffer)], "GpuScene gbuffer");
		uploadList(mLists[static_cast<size_t>(List::Shadow)], "GpuScene shadow");
		uploadList(mLists[static_cast<size_t>(List::Glass)], "GpuScene glass");

		// OutlineComponent is added and removed straight on the registry, with no event, so the outlined set
		// is compared every frame. It is a handful of entities.
		std::vector<ecss::EntityId> outlined;
		for (auto [entity, outline] : ECSHandler::registry().view<const OutlineComponent>()) {
			outlined.push_back(entity);
		}
		if (rebuilt || !pending.empty() || outlined != mOutlined) {
			mOutlined = std::move(outlined);
			rebuildOutline(materials);
		}

		// After the lists: every mesh an instance refers to already exists by then.
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

	void GpuScene::markEntity(const ecss::EntityId entity) {
		std::lock_guard lock(mPendingMutex);
		mPending.push_back(entity);
	}

	void GpuScene::rebuildAll(GpuMaterialTable& materials) {
		for (const auto list : { List::GBuffer, List::Shadow, List::Glass }) {
			auto& data = mLists[static_cast<size_t>(list)];
			data.groups.clear();
			data.groupStates.clear();
			data.groupByKey.clear();
			data.instanceMirror.clear();
			data.batchMirror.clear();
			data.batchStates.clear();
			data.liveInstances = 0;
			data.ownedInstances = 0;
			data.ownedBatches = 0;
			// The mirrors are uploaded whole, into the buffers they already have when those are large enough.
			data.instanceDirtyLo = 0;
			data.instanceDirtyHi = UINT32_MAX;
			data.batchDirtyLo = 0;
			data.batchDirtyHi = UINT32_MAX;
		}
		mEntityRefs.clear();
		for (auto [entity, transform, meshComp] : ECSHandler::registry().view<const ComponentsModule::TransformMatComp, const MeshComponent>()) {
			if (transform && meshComp) {
				addEntity(entity, materials);
			}
		}
		// Adding one at a time grew every batch by doubling; lay them out again at the size they ended up.
		for (const auto list : { List::GBuffer, List::Shadow, List::Glass }) {
			compact(mLists[static_cast<size_t>(list)]);
		}
	}

	bool GpuScene::validate() const {
		for (const auto& [entity, refs] : mEntityRefs) {
			for (const auto& ref : refs) {
				const auto& data = mLists[static_cast<size_t>(ref.list)];
				if (ref.group >= data.groups.size() || ref.batch >= data.groups[ref.group].batchCount) {
					return false;
				}
				const uint32_t global = data.groups[ref.group].firstBatch + ref.batch;
				if (global >= data.batchMirror.size() || ref.slot >= data.batchMirror[global].instanceCount) {
					return false;
				}
				const auto& batch = data.batchMirror[global];
				if (batch.firstInstance + ref.slot >= data.instanceMirror.size()
					|| data.instanceMirror[batch.firstInstance + ref.slot].entity != entity
					|| data.batchStates[global].owners[ref.slot].entity != entity) {
					return false;
				}
			}
		}
		for (const auto list : { List::GBuffer, List::Shadow, List::Glass }) {
			const auto& data = mLists[static_cast<size_t>(list)];
			size_t live = 0;
			size_t ownedInstances = 0;
			for (size_t b = 0; b < data.batchMirror.size(); ++b) {
				if (data.batchStates[b].owners.size() != data.batchMirror[b].instanceCount
					|| data.batchMirror[b].instanceCount > data.batchStates[b].capacity) {
					return false;
				}
				live += data.batchMirror[b].instanceCount;
				ownedInstances += data.batchStates[b].capacity;
			}
			size_t ownedBatches = 0;
			for (const auto& state : data.groupStates) {
				ownedBatches += state.batchCapacity;
			}
			if (live != data.liveInstances || ownedInstances != data.ownedInstances || ownedBatches != data.ownedBatches
				|| data.ownedInstances > data.instanceMirror.size() || data.ownedBatches > data.batchMirror.size()) {
				return false;
			}
		}
		return true;
	}

	void GpuScene::compact(ListData& data) {
		++mCompactions;
		// Groups back to back and their batches with them, each with a quarter spare. References hold a group,
		// a batch inside it and a slot inside that, so none of them moves.
		std::vector<InstanceRecord> packed;
		std::vector<BatchRecord> batches;
		std::vector<BatchState> states;
		packed.reserve(data.liveInstances + data.liveInstances / 4 + data.batchMirror.size() * 4);
		data.ownedInstances = 0;
		data.ownedBatches = 0;
		for (size_t g = 0; g < data.groups.size(); ++g) {
			auto& draw = data.groups[g];
			const auto first = static_cast<uint32_t>(batches.size());
			const uint32_t batchCapacity = draw.batchCount + draw.batchCount / 4 + 4;
			batches.resize(first + batchCapacity);
			states.resize(first + batchCapacity);
			for (uint32_t i = 0; i < draw.batchCount; ++i) {
				auto batch = data.batchMirror[draw.firstBatch + i];
				auto& state = data.batchStates[draw.firstBatch + i];
				const auto start = static_cast<uint32_t>(packed.size());
				packed.insert(packed.end(), data.instanceMirror.begin() + batch.firstInstance, data.instanceMirror.begin() + batch.firstInstance + batch.instanceCount);
				state.capacity = batch.instanceCount + batch.instanceCount / 4 + 4;
				packed.resize(start + state.capacity);
				batch.firstInstance = start;
				data.ownedInstances += state.capacity;
				batches[first + i] = batch;
				states[first + i] = std::move(state);
			}
			draw.firstBatch = first;
			data.groupStates[g].batchCapacity = batchCapacity;
			data.ownedBatches += batchCapacity;
		}
		data.instanceMirror = std::move(packed);
		data.batchMirror = std::move(batches);
		data.batchStates = std::move(states);
		data.instanceDirtyLo = 0;
		data.instanceDirtyHi = UINT32_MAX;
		data.batchDirtyLo = 0;
		data.batchDirtyHi = UINT32_MAX;
	}

	void GpuScene::addEntity(const ecss::EntityId entity, GpuMaterialTable& materials) {
		static const ComponentsModule::MaterialComponent kNoMaterial;
		auto& registry = ECSHandler::registry();
		const auto meshComp = registry.pinComponent<const MeshComponent>(entity);
		if (!meshComp || !registry.hasComponent<ComponentsModule::TransformMatComp>(entity)) {
			return;
		}
		const auto matComp = registry.pinComponent<const MaterialComponent>(entity);
		auto& refs = mEntityRefs[entity];

		meshComp->forEachDrawMesh(matComp.get(), [&](const RenderMeshData& mesh, const MaterialComponent* mat) {
			const auto& material = mat ? *mat : kNoMaterial;
			const bool transparent = mat && mat->transparent;
			const uint32_t materialIndex = materials.indexFor(material);

			// Shadow casters: a transparent one casts through the glass proxy when there is one.
			const auto& caster = transparent && mGlassShadowProxy ? mGlassShadowProxy : mesh;
			if (caster.indexCount != 0) {
				const bool dithered = material.opacity > 0.f && material.opacity < 0.999f;
				const float cutoff = MaterialRules::cutoutCutoff(material.alphaCutoff, material.alphaFromMaps, material.fragmentShader);
				const bool cutout = material.alphaFromMaps || cutoff > 0.f || dithered;
				DrawGroup group;
				group.maskTexture = !cutout ? TextureHandle{}
					: material.alphaFromMaps ? MaterialRules::maskTexture(material.materials) : MaterialRules::diffuseTexture(material.materials);
				group.cutout = cutout;
				group.transparent = transparent;
				const auto key = std::to_string(group.maskTexture.id) + (cutout ? "|c" : "|o") + (transparent ? "t" : "s");
				const auto index = groupFor(List::Shadow, key, std::move(group));
				addInstance(refs, entity, List::Shadow, index, caster.meshId, { entity, caster.meshId, materialIndex, 0 });
			}

			if (mesh.indexCount == 0) {
				return;
			}
			if (transparent) {
				// Glass is one batch whatever the mesh: sortGlass orders every instance by depth.
				const auto index = groupFor(List::Glass, "glass", DrawGroup{});
				addInstance(refs, entity, List::Glass, index, 0, { entity, mesh.meshId, materialIndex, 0 });
				return;
			}
			DrawGroup group;
			group.vertexShader = material.vertexShader.empty() ? std::string(ComponentsModule::DefaultGBufferVS) : material.vertexShader;
			group.fragmentShader = material.fragmentShader.empty() ? std::string(ComponentsModule::DefaultGBufferFS) : material.fragmentShader;
			group.textures = material.materials;
			group.textureKey = MaterialRules::textureKey(material.materials);
			const auto key = group.vertexShader + '|' + group.fragmentShader + '|' + std::to_string(group.textureKey);
			const auto index = groupFor(List::GBuffer, key, std::move(group));
			addInstance(refs, entity, List::GBuffer, index, mesh.meshId, { entity, mesh.meshId, materialIndex, 0 });
		});

		if (refs.empty()) {
			mEntityRefs.erase(entity);
			return;
		}
		mMaxEntity = (std::max)(mMaxEntity, entity);
	}

	void GpuScene::removeEntity(const ecss::EntityId entity) {
		const auto it = mEntityRefs.find(entity);
		if (it == mEntityRefs.end()) {
			return;
		}
		// Each removal can move another slot into the hole and rewrites that slot's reference, this entity's own
		// included, so every reference is read at the moment it is removed.
		auto& refs = it->second;
		for (size_t i = 0; i < refs.size(); ++i) {
			removeInstance(refs[i]);
		}
		mEntityRefs.erase(it);
	}

	uint32_t GpuScene::groupFor(const List list, const std::string& key, DrawGroup&& prototype) {
		auto& data = mLists[static_cast<size_t>(list)];
		const auto [it, inserted] = data.groupByKey.try_emplace(key, static_cast<uint32_t>(data.groups.size()));
		if (inserted) {
			prototype.firstBatch = static_cast<uint32_t>(data.batchMirror.size());
			prototype.batchCount = 0;
			data.groups.push_back(std::move(prototype));
			data.groupStates.push_back({});
			growGroup(data, it->second, 4);
		}
		return it->second;
	}

	void GpuScene::addInstance(std::vector<InstanceRef>& refs, const ecss::EntityId entity, const List list, const uint32_t group,
		const uint32_t batchKey, const InstanceRecord& record)
	{
		auto& data = mLists[static_cast<size_t>(list)];
		auto& groupState = data.groupStates[group];
		auto found = groupState.batchByKey.find(batchKey);
		if (found == groupState.batchByKey.end()) {
			if (data.groups[group].batchCount == groupState.batchCapacity) {
				growGroup(data, group, groupState.batchCapacity * 2);
			}
			const uint32_t local = data.groups[group].batchCount++;
			found = groupState.batchByKey.emplace(batchKey, local).first;
			const uint32_t global = data.groups[group].firstBatch + local;
			data.batchMirror[global] = { static_cast<uint32_t>(data.instanceMirror.size()), 0, record.mesh, 0 };
			data.batchStates[global] = {};
		}

		const uint32_t global = data.groups[group].firstBatch + found->second;
		auto& state = data.batchStates[global];
		if (data.batchMirror[global].instanceCount == state.capacity) {
			growBatch(data, global, (std::max)(state.capacity * 2, 4u));
		}
		auto& batch = data.batchMirror[global];
		const uint32_t slot = batch.instanceCount++;
		const uint32_t index = batch.firstInstance + slot;
		data.instanceMirror[index] = record;
		state.owners.push_back({ entity, static_cast<uint32_t>(refs.size()) });
		refs.push_back({ list, group, found->second, slot });
		++data.liveInstances;

		data.instanceDirtyLo = (std::min)(data.instanceDirtyLo, index);
		data.instanceDirtyHi = (std::max)(data.instanceDirtyHi, index + 1);
		data.batchDirtyLo = (std::min)(data.batchDirtyLo, global);
		data.batchDirtyHi = (std::max)(data.batchDirtyHi, global + 1);
	}

	void GpuScene::removeInstance(const InstanceRef& ref) {
		auto& data = mLists[static_cast<size_t>(ref.list)];
		const uint32_t global = data.groups[ref.group].firstBatch + ref.batch;
		auto& batch = data.batchMirror[global];
		auto& state = data.batchStates[global];
		const uint32_t last = batch.instanceCount - 1;
		if (ref.slot != last) {
			// The last instance fills the hole, and its owner is told where it went.
			data.instanceMirror[batch.firstInstance + ref.slot] = data.instanceMirror[batch.firstInstance + last];
			const auto moved = state.owners[last];
			state.owners[ref.slot] = moved;
			mEntityRefs[moved.entity][moved.ref].slot = ref.slot;
			const uint32_t index = batch.firstInstance + ref.slot;
			data.instanceDirtyLo = (std::min)(data.instanceDirtyLo, index);
			data.instanceDirtyHi = (std::max)(data.instanceDirtyHi, index + 1);
		}
		state.owners.pop_back();
		--batch.instanceCount;
		--data.liveInstances;
		data.batchDirtyLo = (std::min)(data.batchDirtyLo, global);
		data.batchDirtyHi = (std::max)(data.batchDirtyHi, global + 1);
	}

	void GpuScene::growBatch(ListData& data, const uint32_t batch, const uint32_t capacity) {
		auto& record = data.batchMirror[batch];
		const uint32_t start = static_cast<uint32_t>(data.instanceMirror.size());
		data.instanceMirror.resize(start + capacity);
		std::copy_n(data.instanceMirror.begin() + record.firstInstance, record.instanceCount, data.instanceMirror.begin() + start);
		record.firstInstance = start;
		data.ownedInstances += capacity - data.batchStates[batch].capacity;
		data.batchStates[batch].capacity = capacity;
		data.instanceDirtyLo = (std::min)(data.instanceDirtyLo, start);
		data.instanceDirtyHi = (std::max)(data.instanceDirtyHi, start + capacity);
		data.batchDirtyLo = (std::min)(data.batchDirtyLo, batch);
		data.batchDirtyHi = (std::max)(data.batchDirtyHi, batch + 1);
	}

	void GpuScene::growGroup(ListData& data, const uint32_t group, const uint32_t capacity) {
		auto& draw = data.groups[group];
		const uint32_t first = static_cast<uint32_t>(data.batchMirror.size());
		data.batchMirror.resize(first + capacity);
		data.batchStates.resize(first + capacity);
		for (uint32_t i = 0; i < draw.batchCount; ++i) {
			data.batchMirror[first + i] = data.batchMirror[draw.firstBatch + i];
			data.batchStates[first + i] = std::move(data.batchStates[draw.firstBatch + i]);
			// The old slot stays behind as an empty batch: culled to a command with no instances, owning nothing.
			data.batchMirror[draw.firstBatch + i] = {};
			data.batchStates[draw.firstBatch + i] = {};
		}
		data.batchDirtyLo = (std::min)(data.batchDirtyLo, draw.batchCount ? draw.firstBatch : first);
		data.batchDirtyHi = (std::max)(data.batchDirtyHi, first + capacity);
		draw.firstBatch = first;
		data.ownedBatches += capacity - data.groupStates[group].batchCapacity;
		data.groupStates[group].batchCapacity = capacity;
	}

	void GpuScene::uploadList(ListData& data, const char* name) {
		auto* rhi = Engine::instance()->rhi();
		const auto upload = [&](BufferHandle& buffer, size_t& capacity, const size_t count, const size_t stride, const void* bytes,
			uint32_t& lo, uint32_t& hi)
		{
			if (!buffer || count > capacity) {
				retire(buffer);
				capacity = (std::max)(capacity, size_t{ 256 });
				while (capacity < count) {
					capacity *= 2;
				}
				buffer = createBuffer(capacity * stride, BufferUsage::Storage, CpuAccess::None, nullptr, name);
				lo = 0;
				hi = static_cast<uint32_t>(count);
			}
			hi = (std::min)(hi, static_cast<uint32_t>(count));
			if (lo < hi) {
				rhi->updateBuffer(buffer, lo * stride, (hi - lo) * stride, static_cast<const uint8_t*>(bytes) + lo * stride);
			}
			lo = UINT32_MAX;
			hi = 0;
		};
		upload(data.instances, data.instanceGpuCapacity, data.instanceMirror.size(), sizeof(InstanceRecord), data.instanceMirror.data(),
			data.instanceDirtyLo, data.instanceDirtyHi);
		upload(data.batches, data.batchGpuCapacity, data.batchMirror.size(), sizeof(BatchRecord), data.batchMirror.data(),
			data.batchDirtyLo, data.batchDirtyHi);
		data.instanceCount = static_cast<uint32_t>(data.instanceMirror.size());
		data.batchCount = static_cast<uint32_t>(data.batchMirror.size());
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
		if (view.params.size() < frameSlots()) {
			view.params.resize(frameSlots());
			view.sets.resize(frameSlots());
			view.boundIds.resize(frameSlots());
		}
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
		const auto [base, count] = glassRange();
		if (count == 0 || !mGlassPipeline || !mMeshes || !transforms) {
			return false;
		}
		auto* rhi = Engine::instance()->rhi();

		uint32_t padded = 1;
		while (padded < count) {
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
		const uint32_t slot = prepareView(view, count, count, 0, shapes, 8, 0);
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
			uint32_t base = 0;
		} step;
		step.count = count;
		step.base = base;
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
		dispatch(count);

		rhi->cmdPipelineBarrier(cmd, {
			PipelineStage::Compute,
			PipelineStage::DrawIndirect | PipelineStage::VertexShader,
			AccessMask::ShaderWrite,
			AccessMask::IndirectCommandRead | AccessMask::ShaderRead,
		});
		return true;
	}

	std::pair<uint32_t, uint32_t> GpuScene::glassRange() const {
		const auto& data = mLists[static_cast<size_t>(List::Glass)];
		if (data.groups.empty() || data.groups[0].batchCount == 0) {
			return { 0, 0 };
		}
		const auto& batch = data.batchMirror[data.groups[0].firstBatch];
		return { batch.firstInstance, batch.instanceCount };
	}

	void GpuScene::drawSorted(const CommandListHandle cmd, const View& view) const {
		const auto count = glassRange().second;
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
