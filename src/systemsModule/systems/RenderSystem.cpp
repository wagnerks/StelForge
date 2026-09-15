#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "renderModule/FrameCapture.h"

#include "CameraSystem.h"
#include "OcTreeSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "renderModule/renderPasses/DebugPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "runtimeBridge/RuntimeBridge.h"

#include "componentsModule/TransformComponent.h"
#include "renderModule/TextRenderer.h"
#include "renderModule/renderPasses/GUIPass.h"
#include <debugModule/SystemLoadHelper.h>

#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/ArmatureComponent.h"
#include "renderModule/renderPasses/GeometryPass.h"
#include "renderModule/renderPasses/AnimationComputePass.h"
#include "containersModule/Vector.h"

namespace SFE::SystemsModule {

	namespace {
		// Writes one PNG per pass output plus a crc index, so a comparison script can find the
		// FIRST pass where two backends diverge instead of guessing from the final image.
		class PassDumpWriter {
		public:
			explicit PassDumpWriter(std::string dir) : mDir(std::move(dir)) {
				if (mDir.empty()) {
					return;
				}
				std::error_code ec;
				std::filesystem::create_directories(mDir, ec);
				mIndex.open(std::filesystem::path(mDir) / "passes.csv", std::ios::trunc);
				if (mIndex) {
					mIndex << "index,pass,slot,width,height,crc\n";
				}
			}

			// Stage 0: what every slot holds before any pass has run. Written, so a comparison always has a
			// baseline for a slot that one backend changes in a pass and the other leaves as it was.
			void seed(const Render::FrameBlackboard& frame) {
				if (mDir.empty()) {
					return;
				}
				dumpAll(0, "seed", frame);
			}

			void write(size_t passIndex, const char* passName, const Render::FrameBlackboard& frame) {
				if (mDir.empty()) {
					return;
				}
				dumpAll(passIndex + 1, passName, frame);
			}

		private:
			// Every texture on the blackboard, so each intermediate stage is compared, not a hand-picked
			// few. dump() skips a slot whose content has not changed, so a pass only reports what it wrote.
			void dumpAll(size_t passIndex, const char* passName, const Render::FrameBlackboard& frame) {
				for (const auto& name : frame.textureNames()) {
					const auto tex = frame.texture(name.c_str());
					if (name == Render::FrameId::ShadowCsmDepth) {
						// A depth array: the RHI cannot report its layer count, and the engine uses four cascades.
						for (uint32_t cascade = 0; cascade < 4; ++cascade) {
							dump(passIndex, passName, name + ".cascade" + std::to_string(cascade), tex, cascade);
						}
						continue;
					}
					dump(passIndex, passName, name, tex);
				}
			}

			void dump(size_t passIndex, const char* passName, const std::string& slot, Render::TextureHandle tex, uint32_t layer = 0) {
				auto* rhi = Engine::instance()->rhi();
				if (!tex || !rhi) {
					return;
				}
				std::vector<uint8_t> rgba;
				int width = 0;
				int height = 0;
				if (!rhi->readbackTexture(tex, rgba, width, height, layer)) {
					return;
				}

				// A slot a pass did not change is not written again; the comparison carries the last dump forward.
				const uint32_t crc = Render::crcOfPixels(rgba);
				auto& last = mLastCrc[slot];
				if (last == crc) {
					return;
				}
				last = crc;

				char name[160];
				std::snprintf(name, sizeof(name), "%02d_%s_%s.png", static_cast<int>(passIndex), passName, slot.c_str());
				const auto path = (std::filesystem::path(mDir) / name).string();
				if (Render::writePng(path, rgba, width, height) && mIndex) {
					mIndex << passIndex << ',' << passName << ',' << slot << ','
						<< width << ',' << height << ',' << crc << '\n';
					mIndex.flush();
				}
			}

			std::string mDir;
			std::ofstream mIndex;
			std::unordered_map<std::string, uint32_t> mLastCrc;
		};
	}

	RenderSystem::RenderSystem() : System({ SFE::SystemsModule::TaskType::TRAHSFORM_RELOADED , SFE::SystemsModule::TaskType::ARMATURE_UPDATED, MATERIAL_UPDATED, MESH_UPDATED, ENTITY_DESTROYED }) {
		auto* rhi = Engine::instance()->rhi();
		if (!Engine::instance()->temporalAccumulation()) {
			mRenderData.graphics.taa = false;
		}
		mGpuFrames = (std::max)(1u, rhi->framesInFlight());
		mCameraUbo.resize(mGpuFrames);
		mFrameSet.resize(mGpuFrames);
		constexpr size_t maxDrawSize = 4096;
		mGpuCapacity = maxDrawSize;

		{
			Render::DescriptorSetLayoutDesc frameLayout;
			frameLayout.bindings = {
				{ 5, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Vertex | Render::ShaderStage::Fragment | Render::ShaderStage::Geometry },
				{ 1, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex },
				{ 2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex | Render::ShaderStage::Compute },
				{ 3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex | Render::ShaderStage::Fragment },
				{ 7, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Fragment },
			};
			frameSetLayout = rhi->createDescriptorSetLayout(frameLayout);
			mRenderData.frameSetLayout = frameSetLayout;
		}

		createSceneBuffers(maxDrawSize);
		growBones(4096);
		mScene.init();
		for (uint32_t slot = 0; slot < mGpuFrames; ++slot) {
			createGpuFrameResources(slot);
		}
		bindGpuFrame();

		SFE::Engine::reproTrace("RenderSystem passes");
		mRenderPasses.reserve(std::size(RENDER_PASSES));
		for (size_t i = 0; i < std::size(RENDER_PASSES); ++i) {
			const auto& entry = RENDER_PASSES[i];
			if (!entry.enabled || !entry.create) {
				continue;
			}
			SFE::Engine::reproTrace(entry.name);
			auto* pass = entry.create();
			pass->init(mRenderData);
			pass->setPriority(i);
			mRenderPasses.emplace_back(pass);
			mPassNames.emplace_back(entry.name);
		}
		SFE::Engine::reproTrace("RenderSystem passes ok");

		// ChunksSystem seeds before this ctor; ARMATURE_UPDATED never reached us. Transforms are refreshed on
		// the first frame, and GpuScene starts dirty.
		auto& live = ECSHandler::registry();
		for (auto id : live.getAllEntities()) {
			if (live.hasComponent<ComponentsModule::ArmatureBonesComponent>(id)) {
				markGpuDirty(mGpuDirtyBones, id);
			}
		}
	}

	namespace {
		constexpr int kTaaJitterPhases = 8;

		// Radical inverse of `index` in `base`, in [0, 1).
		float halton(int index, int base) {
			float f = 1.f;
			float r = 0.f;
			while (index > 0) {
				f /= static_cast<float>(base);
				r += f * static_cast<float>(index % base);
				index /= base;
			}
			return r;
		}
	}

	void RenderSystem::update(float_t dt) {
		FUNCTION_BENCHMARK;

		auto* rhi = Engine::instance()->rhi();
		// Systems that ran earlier this frame may have queued asynchronous work (AABB, octree) that the
		// culling below reads; a capture lets it finish so what is drawn does not depend on scheduling.
		if (Engine::instance()->capturingFrame()) {
			ThreadPool::instance()->waitIdle();
		}
		rhi->beginFrame();
		bindGpuFrame();
		auto frameLock = tryLockWorldRead();

		mRenderData.prevPV = mRenderData.current.PV;
		mRenderData.current = mRenderData.next;
		mRenderData.cameraProjection = mRenderData.nextCameraProjection;

		mRenderData.mCameraPos = mRenderData.mNextCameraPos;
		mRenderData.mViewDir = mRenderData.mNextViewDir;
		mRenderData.mCamFrustum = mRenderData.mNextCamFrustum;

		++mRenderData.taaFrame;
		mRenderData.taaJitterX = 0.f;
		mRenderData.taaJitterY = 0.f;

		// TAA resolves nothing without a sub-pixel offset per frame: every frame would sample the
		// same points and the history would already equal the current image. Only the matrices the
		// GPU rasterizes with are jittered -- mRenderData.current stays clean, so prevPV, culling
		// and the post-TAA debug overlay keep working in unjittered screen space.
		RenderMatrices gpuMatrices = mRenderData.current;
		if (mRenderData.graphics.taa) {
			const auto& screen = Engine::instance()->getWindow()->getScreenData();
			if (screen.renderW > 0 && screen.renderH > 0) {
				const int phase = mRenderData.taaFrame % kTaaJitterPhases + 1;
				mRenderData.taaJitterX = halton(phase, 2) - 0.5f;
				mRenderData.taaJitterY = halton(phase, 3) - 0.5f;

				// perspectiveRH_NO gives w = -z_view, so a term on the z column survives the
				// perspective divide as a constant NDC shift, unlike one on the translation column.
				gpuMatrices.projection[2][0] -= 2.f * mRenderData.taaJitterX / static_cast<float>(screen.renderW);
				gpuMatrices.projection[2][1] -= 2.f * mRenderData.taaJitterY / static_cast<float>(screen.renderH);
				gpuMatrices.PV = gpuMatrices.projection * gpuMatrices.view;
			}
		}

		Engine::instance()->rhi()->updateBuffer(cameraMatricesUBO, 0, sizeof(RenderMatrices), &gpuMatrices);

		{
			auto playerCamera = ECSHandler::getSystem<CameraSystem>()->getCurrentCamera();
			const auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(playerCamera);
			// The copy the transform worker publishes, not TransformComponent itself: the worker rewrites that
			// matrix while this thread reads it, and a moving camera now and then got a half-written view.
			// Replacing the copy waits for pins, so a pinned copy is whole.
			Math::Mat4 cameraTransform{ 1.f };
			if (const auto copy = ECSHandler::registry().pinComponent<const ComponentsModule::TransformMatComp>(playerCamera)) {
				cameraTransform = copy->mTransform;
			}
			else if (const auto transformComp = ECSHandler::registry().pinComponent<const TransformComponent>(playerCamera)) {
				cameraTransform = transformComp->copyTransform(); // before the first reload reached RenderSystem
			}

			mRenderData.nextCameraProjection = cameraComp->getProjection();
			mRenderData.next.projection = mRenderData.nextCameraProjection.getProjectionsMatrix();
			mRenderData.next.view = inverse(cameraTransform);
			mRenderData.next.PV = mRenderData.next.projection * mRenderData.next.view;

			mRenderData.mNextCameraPos = cameraTransform[3].xyz;
			const Math::Vec3 forward = -cameraTransform[2];
			mRenderData.mNextViewDir = normalize(forward);
			mRenderData.mNextCamFrustum = FrustumModule::createFrustum(mRenderData.next.PV);
		}

		if (frameLock) {
			if (mRefreshTransforms) {
				// A reload that finished before we subscribed -- or while the constructor ran -- never reached
				// notify, so the copies are taken from the source once, now that every later reload does.
				mRefreshTransforms = false;
				auto& live = ECSHandler::registry();
				for (auto [id, matComp, transform] : live.view<ComponentsModule::TransformMatComp, const TransformComponent>()) {
					if (matComp && transform) {
						matComp->mTransform = transform->copyTransform();
						markGpuDirty(mGpuDirtyTransforms, id);
					}
				}
			}
			uploadDirtyGpuData();
			mScene.update(mMaterials);
			// An entity can join the scene after this frame's dirty transforms were taken, its id past the
			// buffer; the grow uploads every live transform, its own included.
			ensureGpuCapacity(mScene.maxEntity());
			// Before the frame is recorded, so new materials go up with the frame's queued uploads; the passes'
			// own upload() calls then find nothing to send. Sent after the list began, Vulkan would submit and
			// wait for each one.
			mMaterials.upload();
		}

		renderFrame();
	}

	void RenderSystem::debugUpdate(float dt) {
	}

	void RenderSystem::bindGpuFrame() {
		if (mGpuFrames == 0) {
			return;
		}
		auto* rhi = Engine::instance()->rhi();
		const uint32_t slot = rhi->frameInFlight() % mGpuFrames;
		cameraMatricesUBO = mCameraUbo[slot];
		frameSet = mFrameSet[slot];
		mRenderData.transformsSSBO = transforms;
		mRenderData.bonesSSBO = bones;
		mRenderData.cameraMatricesUBO = cameraMatricesUBO;
	}

	void RenderSystem::createGpuFrameResources(const uint32_t slot) {
		auto* rhi = Engine::instance()->rhi();
		Render::BufferDesc uboDesc;
		uboDesc.size = sizeof(RenderMatrices);
		uboDesc.usage = Render::BufferUsage::Uniform;
		uboDesc.cpuAccess = Render::CpuAccess::Write;
		uboDesc.debugName = "CameraMatricesUBO";
		mCameraUbo[slot] = rhi->createBuffer(uboDesc);

		mFrameSet[slot] = rhi->allocateDescriptorSet(frameSetLayout);
		rhi->updateDescriptorSets({
			{ mFrameSet[slot], 5, {{ mCameraUbo[slot] }} },
			{ mFrameSet[slot], 1, {{ transforms }} },
			{ mFrameSet[slot], 2, {{ bones }} },
		});
	}

	void RenderSystem::createSceneBuffers(const size_t capacity) {
		auto* rhi = Engine::instance()->rhi();
		// CpuAccess::None: device-local, written only through updateBuffer, which on Vulkan is copied
		// in the frame's own command list instead of racing the frames still in flight.
		Render::BufferDesc transformDesc;
		transformDesc.size = sizeof(Math::Mat4) * capacity;
		transformDesc.usage = Render::BufferUsage::Storage;
		transformDesc.cpuAccess = Render::CpuAccess::None;
		transformDesc.debugName = "TransformsSSBO";
		transforms = rhi->createBuffer(transformDesc);

		mGpuCapacity = capacity;
	}

	void RenderSystem::ensureGpuCapacity(ecss::EntityId id) {
		const size_t needed = static_cast<size_t>(id) + 1;
		if (needed <= mGpuCapacity) {
			return;
		}

		size_t newCap = mGpuCapacity < 4096 ? 4096 : mGpuCapacity;
		while (newCap < needed) {
			newCap *= 2;
		}

		auto* rhi = Engine::instance()->rhi();
		// The RHI keeps a destroyed buffer until the frames that may still bind it have finished.
		rhi->destroyBuffer(transforms);
		createSceneBuffers(newCap);
		for (uint32_t slot = 0; slot < mGpuFrames; ++slot) {
			rhi->updateDescriptorSets({ { mFrameSet[slot], 1, {{ transforms }} } });
		}
		bindGpuFrame();

		// The new buffers start empty: everything live goes up again.
		std::vector<ecss::EntityId> ids;
		for (auto [entId, comp] : ECSHandler::registry().view<const ComponentsModule::TransformMatComp>()) {
			ids.push_back(entId);
		}
		std::sort(ids.begin(), ids.end());
		uploadTransforms(ids);
	}

	RenderSystem::BoneRange RenderSystem::boneRange(const ecss::EntityId entity) {
		// An id destroyed and taken again before the release was processed: the range held is the old entity's,
		// and releasing it later would free it under the new one.
		bool destroyed = false;
		{
			std::lock_guard lock(mGpuDirtyMutex);
			if (const auto it = std::find(mDestroyedBones.begin(), mDestroyedBones.end(), entity); it != mDestroyedBones.end()) {
				mDestroyedBones.erase(it);
				destroyed = true;
			}
		}
		if (destroyed) {
			releaseBones(entity);
		}
		if (const auto it = mBoneRanges.find(entity); it != mBoneRanges.end()) {
			return it->second;
		}
		auto& registry = ECSHandler::registry();
		uint32_t count = 0;
		if (const auto armature = registry.pinComponent<const ComponentsModule::ArmatureComponent>(entity)) {
			count = static_cast<uint32_t>(armature->armature.bones.size());
		}
		if (count == 0) {
			if (const auto pose = registry.pinComponent<const ComponentsModule::ArmatureBonesComponent>(entity)) {
				count = static_cast<uint32_t>(pose->boneMatrices.size());
			}
		}
		if (count == 0) {
			return {};
		}

		BoneRange range{ 0, count };
		const auto fit = std::find_if(mFreeBoneRanges.begin(), mFreeBoneRanges.end(), [count](const BoneRange& r) { return r.count >= count; });
		if (fit != mFreeBoneRanges.end()) {
			range.offset = fit->offset;
			fit->offset += count;
			fit->count -= count;
			if (fit->count == 0) {
				mFreeBoneRanges.erase(fit);
			}
		}
		else {
			range.offset = static_cast<uint32_t>(mBonesUsed);
			mBonesUsed += count;
			if (mBonesUsed > mBoneCapacity) {
				size_t capacity = mBoneCapacity;
				while (capacity < mBonesUsed) {
					capacity *= 2;
				}
				mBoneRanges.emplace(entity, range);
				growBones(capacity); // uploads every range, this one included
				return range;
			}
		}
		mBoneRanges.emplace(entity, range);
		// Its pose goes up with the next upload, until an animation writes it on the GPU.
		markGpuDirty(mGpuDirtyBones, entity);
		return range;
	}

	void RenderSystem::releaseBones(const ecss::EntityId entity) {
		const auto it = mBoneRanges.find(entity);
		if (it == mBoneRanges.end()) {
			return;
		}
		const auto range = it->second;
		mBoneRanges.erase(it);
		auto next = std::lower_bound(mFreeBoneRanges.begin(), mFreeBoneRanges.end(), range.offset,
			[](const BoneRange& r, uint32_t offset) { return r.offset < offset; });
		next = mFreeBoneRanges.insert(next, range);
		if (next + 1 != mFreeBoneRanges.end() && next->offset + next->count == (next + 1)->offset) {
			next->count += (next + 1)->count;
			mFreeBoneRanges.erase(next + 1);
		}
		if (next != mFreeBoneRanges.begin() && (next - 1)->offset + (next - 1)->count == next->offset) {
			(next - 1)->count += next->count;
			mFreeBoneRanges.erase(next);
		}
	}

	void RenderSystem::growBones(const size_t capacity) {
		auto* rhi = Engine::instance()->rhi();
		// The RHI keeps a destroyed buffer until the frames that may still bind it have finished.
		rhi->destroyBuffer(bones);
		Render::BufferDesc boneDesc;
		boneDesc.size = sizeof(Math::Mat4) * capacity;
		boneDesc.usage = Render::BufferUsage::Storage;
		boneDesc.cpuAccess = Render::CpuAccess::None;
		boneDesc.debugName = "BonesSSBO";
		bones = rhi->createBuffer(boneDesc);
		mBoneCapacity = capacity;
		for (uint32_t slot = 0; slot < mGpuFrames; ++slot) {
			if (mFrameSet[slot]) {
				rhi->updateDescriptorSets({ { mFrameSet[slot], 2, {{ bones }} } });
			}
		}
		bindGpuFrame();

		// Offsets stay where they were; the new buffer starts empty, so every pose goes up again.
		std::vector<ecss::EntityId> ids;
		ids.reserve(mBoneRanges.size());
		for (const auto& [entity, range] : mBoneRanges) {
			ids.push_back(entity);
		}
		uploadBones(ids);
	}

	void RenderSystem::markGpuDirty(std::vector<ecss::EntityId>& list, const ecss::EntityId id) {
		std::lock_guard lock(mGpuDirtyMutex);
		list.push_back(id);
	}

	void RenderSystem::uploadTransforms(const std::vector<ecss::EntityId>& sortedIds) {
		auto* rhi = Engine::instance()->rhi();
		auto& registry = ECSHandler::registry();
		// Consecutive ids go up as one range, so a spawned chunk is one copy rather than one per entity.
		std::vector<Math::Mat4> run;
		ecss::EntityId runStart = 0;
		const auto flush = [&] {
			if (!run.empty()) {
				rhi->updateBuffer(transforms, static_cast<size_t>(runStart) * sizeof(Math::Mat4), run.size() * sizeof(Math::Mat4), run.data());
				run.clear();
			}
		};
		for (const auto id : sortedIds) {
			if (static_cast<size_t>(id) >= mGpuCapacity) {
				break;
			}
			const auto comp = registry.pinComponent<const ComponentsModule::TransformMatComp>(id);
			if (!comp) {
				flush();
				continue;
			}
			if (!run.empty() && id != runStart + run.size()) {
				flush();
			}
			if (run.empty()) {
				runStart = id;
			}
			run.push_back(comp->mTransform);
		}
		flush();
	}

	void RenderSystem::uploadBones(const std::vector<ecss::EntityId>& sortedIds) {
		auto* rhi = Engine::instance()->rhi();
		auto& registry = ECSHandler::registry();
		for (const auto id : sortedIds) {
			const auto armature = registry.pinComponent<const ComponentsModule::ArmatureBonesComponent>(id);
			if (!armature || armature->boneMatrices.empty()) {
				continue;
			}
			// A playing animation is written straight into the buffer by AnimationComputePass.
			if (const auto anim = registry.pinComponent<const ComponentsModule::AnimationComponent>(id);
				anim && anim->mCurrentAnimation && anim->mPlay) {
				continue;
			}
			const auto range = boneRange(id);
			const size_t boneCount = (std::min)(armature->boneMatrices.size(), static_cast<size_t>(range.count));
			if (boneCount == 0) {
				continue;
			}
			rhi->updateBuffer(bones, static_cast<size_t>(range.offset) * sizeof(Math::Mat4), boneCount * sizeof(Math::Mat4), armature->boneMatrices.data());
		}
	}

	void RenderSystem::uploadDirtyGpuData() {
		std::vector<ecss::EntityId> dirtyTransforms;
		std::vector<ecss::EntityId> dirtyBones;
		std::vector<ecss::EntityId> destroyed;
		{
			std::lock_guard lock(mGpuDirtyMutex);
			dirtyTransforms.swap(mGpuDirtyTransforms);
			dirtyBones.swap(mGpuDirtyBones);
			destroyed.swap(mDestroyedBones);
		}
		// Before the uploads: a reused id takes a range of its own when it is next asked for.
		for (const auto entity : destroyed) {
			releaseBones(entity);
		}
		if (dirtyTransforms.empty() && dirtyBones.empty()) {
			return;
		}
		for (auto* ids : { &dirtyTransforms, &dirtyBones }) {
			std::sort(ids->begin(), ids->end());
			ids->erase(std::unique(ids->begin(), ids->end()), ids->end());
		}

		const size_t capacityBefore = mGpuCapacity;
		ecss::EntityId maxId = 0;
		if (!dirtyTransforms.empty()) {
			maxId = dirtyTransforms.back();
		}
		ensureGpuCapacity(maxId);
		if (mGpuCapacity == capacityBefore) {
			uploadTransforms(dirtyTransforms); // otherwise the grow already uploaded everything live
		}
		uploadBones(dirtyBones);
	}

	void RenderSystem::notify(Task task) {
		if (task.type == TRAHSFORM_RELOADED) {
			auto& registry = ECSHandler::registry();
			const bool first = !registry.hasComponent<ComponentsModule::TransformMatComp>(task.entity);
			registry.addComponent<ComponentsModule::TransformMatComp>(task.entity, static_cast<TransformComponent*>(task.customData)->copyTransform());
			markGpuDirty(mGpuDirtyTransforms, task.entity);
			if (first) {
				// The scene skips an entity with no transform yet, so one whose mesh came first is read again now.
				mScene.markEntity(task.entity);
			}
		}
		else if (task.type == ARMATURE_UPDATED) {
			markGpuDirty(mGpuDirtyBones, task.entity);
			// Its instances carry the bone offset, which the armature decides.
			mScene.markEntity(task.entity);
		}
		else if (task.type == MATERIAL_UPDATED) {
			mScene.markEntity(task.entity);
		}
		else if (task.type == MESH_UPDATED) {
			mScene.markEntity(task.entity);
		}
		else if (task.type == ENTITY_DESTROYED) {
			mScene.markEntity(task.entity);
			std::lock_guard lock(mGpuDirtyMutex);
			mDestroyedBones.push_back(task.entity);
		}
	}

	bool RenderSystem::shouldRunPass(size_t passIndex) const
	{
		if (passIndex >= mPassNames.size()) {
			return true;
		}

		const char* name = mPassNames[passIndex];
		const auto& gfx = mRenderData.graphics;
		if (std::strcmp(name, "SSAO") == 0) {
			return gfx.ssao;
		}
		if (std::strcmp(name, "SSR") == 0) {
			return gfx.ssr;
		}
		if (std::strcmp(name, "Volumetric") == 0) {
			return gfx.volumetric;
		}
		if (std::strcmp(name, "TAA") == 0) {
			return gfx.taa;
		}
		if (std::strcmp(name, "DoF") == 0) {
			return gfx.dof;
		}
		if (std::strcmp(name, "Sheen") == 0) {
			return gfx.sheen;
		}
		if (std::strcmp(name, "SSS") == 0) {
			return gfx.sss;
		}
		if (std::strcmp(name, "ContactGI") == 0) {
			return gfx.contactGi;
		}

		if (std::strcmp(name, "ImGui") == 0) {
			return Engine::instance()->presentToScreen();
		}

		auto* engine = Engine::instance();
		if (engine->publishEditorDisplay() && !engine->presentToScreen()) {
			if (std::strcmp(name, "Debug") == 0) {
				return false;
			}
		}
		return true;
	}

	void RenderSystem::renderFrame()
	{
		auto* info = RenderDebugInfo::instance();
		info->objectsRendered = 0;
		info->verticesRendered = 0;
		info->drawCalls = 0;
		info->passCount = 0;

		const auto tFrame = std::chrono::high_resolution_clock::now();

		auto rhi = Engine::instance()->rhi();

		// One diagnostic frame can snapshot every pass in turn. That needs each pass submitted on
		// its own so the GPU has actually produced the image before it is read back, which is far
		// too expensive for a normal frame -- hence the narrow condition.
		++mRenderedFrames;
		auto* engine = Engine::instance();
		const bool dumpPasses = engine->capturingFrame()
			&& !engine->passDumpDir().empty()
			&& mRenderedFrames == engine->captureFrame();
		PassDumpWriter passDump(dumpPasses ? engine->passDumpDir() : std::string{});
		passDump.seed(mRenderData.frame);

		// No frame-wide bind here: every pass binds its own sets against its pipeline, as Vulkan requires, and a bind
		// with no pipeline would be kept by OpenGL alone and hide a pass that forgot one.
		auto cmd = rhi->beginCommandList(Render::CommandListType::Graphics);
		for (size_t i = 0; i < mRenderPasses.size(); ++i) {
			if (!shouldRunPass(i)) {
				continue;
			}
			const auto tPass = std::chrono::high_resolution_clock::now();
			mRenderPasses[i]->render(mRenderData, cmd);
			if (info->passCount < 16) {
				info->passTimes[info->passCount] = {
					i < mPassNames.size() ? mPassNames[i] : "pass",
					std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - tPass).count()
				};
				++info->passCount;
			}

			if (dumpPasses) {
				rhi->endCommandList(cmd);
				rhi->submit(cmd);
				rhi->waitIdle();
				passDump.write(i, i < mPassNames.size() ? mPassNames[i] : "pass", mRenderData.frame);
				cmd = rhi->beginCommandList(Render::CommandListType::Graphics);
			}
		}
		rhi->endCommandList(cmd);

		const auto tSubmit = std::chrono::high_resolution_clock::now();
		rhi->submit(cmd);
		info->submitMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - tSubmit).count();
		info->gpuMs = static_cast<float>(rhi->lastGpuMs());
		{
			auto presentTex = mRenderData.frame.texture(Render::FrameId::Final);
			auto presentSampler = mRenderData.frame.sampler(Render::FrameId::Final);
			if (!presentTex) {
				presentTex = mRenderData.frame.texture(Render::FrameId::GBufferAlbedo);
				presentSampler = mRenderData.frame.sampler(Render::FrameId::GBufferAlbedo);
			}
			if (presentTex) {
				rhi->setPresentBlitSource(presentTex, presentSampler);
			}
		}
		if (Engine::instance()->publishEditorDisplay()) {
			if (RuntimeBridge::RuntimeBridge::isAlive()) {
				RuntimeBridge::RuntimeBridge::instance()->streamPresentedFrame();
			}
		}

		const auto tPresent = std::chrono::high_resolution_clock::now();
		rhi->present();
		info->presentMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - tPresent).count();
		info->cpuMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - tFrame).count();
	}

	RenderSystem::~RenderSystem() {
		for (const auto renderPass : mRenderPasses) {
			delete renderPass;
		}
	}
}
