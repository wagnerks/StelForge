#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "CameraSystem.h"
#include "imgui.h"
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
#include "containersModule/Vector.h"
#include "renderModule/renderPasses/FinalPass.h"

namespace SFE::SystemsModule {

	RenderSystem::RenderSystem() : System({ SFE::SystemsModule::TaskType::TRAHSFORM_RELOADED , SFE::SystemsModule::TaskType::ARMATURE_UPDATED, MATERIAL_UPDATED, MESH_UPDATED }) {
		mRenderPasses.reserve(std::size(RENDER_PASSES));
		for (size_t i = 0; i < std::size(RENDER_PASSES); ++i) {
			const auto& entry = RENDER_PASSES[i];
			if (!entry.enabled || !entry.create) {
				continue;
			}
			auto* pass = entry.create();
			pass->init(mRenderData);
			pass->setPriority(i);
			mRenderPasses.emplace_back(pass);
			mPassNames.emplace_back(entry.name);
		}
	
		{
			Render::BufferDesc uboDesc;
			uboDesc.size = sizeof(RenderMatrices);
			uboDesc.usage = Render::BufferUsage::Uniform;
			uboDesc.cpuAccess = Render::CpuAccess::Write;
			uboDesc.debugName = "CameraMatricesUBO";
			cameraMatricesUBO = Engine::instance()->rhi()->createBuffer(uboDesc);
		}

		constexpr size_t maxDrawSize = 4096;
		mGpuCapacity = maxDrawSize;
		{
			Render::BufferDesc ssbDesc;
			ssbDesc.size = sizeof(Math::Mat4) * maxDrawSize;
			ssbDesc.usage = Render::BufferUsage::Storage;
			ssbDesc.cpuAccess = Render::CpuAccess::Write;
			ssbDesc.debugName = "TransformsSSBO";
			transforms = Engine::instance()->rhi()->createBuffer(ssbDesc);
		}

		{
			Render::BufferDesc ssbDesc;
			ssbDesc.size = sizeof(SFE::Math::Mat4) * 100 * maxDrawSize;
			ssbDesc.usage = Render::BufferUsage::Storage;
			ssbDesc.cpuAccess = Render::CpuAccess::Write;
			ssbDesc.debugName = "BonesSSBO";
			bones = Engine::instance()->rhi()->createBuffer(ssbDesc);
		}

		{
			Render::DescriptorSetLayoutDesc frameLayout;
			frameLayout.bindings = {
				{ 5, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Vertex | Render::ShaderStage::Fragment | Render::ShaderStage::Geometry },
				{ 1, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex },
				{ 2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex | Render::ShaderStage::Compute },
			};
			frameSetLayout = Engine::instance()->rhi()->createDescriptorSetLayout(frameLayout);
			frameSet = Engine::instance()->rhi()->allocateDescriptorSet(frameSetLayout);
			Engine::instance()->rhi()->updateDescriptorSets({
				{ frameSet, 5, Render::DescriptorType::UniformBuffer, {{ cameraMatricesUBO }} },
				{ frameSet, 1, Render::DescriptorType::StorageBuffer, {{ transforms }} },
				{ frameSet, 2, Render::DescriptorType::StorageBuffer, {{ bones }} },
			});
			mRenderData.transformsSSBO = transforms;
			mRenderData.bonesSSBO = bones;
			mRenderData.cameraMatricesUBO = cameraMatricesUBO;
		}

		ECSHandler::drawRegistry(0).registerArray<ComponentsModule::TransformMatComp, MeshComponent, MaterialComponent, ComponentsModule::ArmatureBonesComponent>(0);
		ECSHandler::drawRegistry(1).registerArray<ComponentsModule::TransformMatComp, MeshComponent, MaterialComponent, ComponentsModule::ArmatureBonesComponent>(0);

		// init gpu timer queries
		//glGenQueries(2, mGpuTimeQueries);
	}

	void RenderSystem::update(float_t dt) {
		FUNCTION_BENCHMARK;

		mRenderData.prevPV = mRenderData.current.PV;
		mRenderData.current = mRenderData.next;
		mRenderData.cameraProjection = mRenderData.nextCameraProjection;

		mRenderData.mCameraPos = mRenderData.mNextCameraPos;
		mRenderData.mViewDir = mRenderData.mNextViewDir;
		mRenderData.mCamFrustum = mRenderData.mNextCamFrustum;

		++mRenderData.taaFrame;
		if (mRenderData.graphics.taa) {
			const auto& screen = Engine::instance()->getWindow()->getScreenData();
			auto halton = [](uint32_t index, uint32_t base) {
				float f = 1.f;
				float r = 0.f;
				while (index > 0) {
					f /= static_cast<float>(base);
					r += f * static_cast<float>(index % base);
					index /= base;
				}
				return r;
			};
			const uint32_t idx = static_cast<uint32_t>(mRenderData.taaFrame % 16) + 1u;
			const float rw = screen.renderW > 0 ? static_cast<float>(screen.renderW) : 1.f;
			const float rh = screen.renderH > 0 ? static_cast<float>(screen.renderH) : 1.f;
			const float jx = (halton(idx, 2u) - 0.5f) * 2.f / rw;
			const float jy = (halton(idx, 3u) - 0.5f) * 2.f / rh;
			mRenderData.current.projection[2][0] += jx;
			mRenderData.current.projection[2][1] += jy;
			mRenderData.current.PV = mRenderData.current.projection * mRenderData.current.view;
		}

		Engine::instance()->rhi()->updateBuffer(cameraMatricesUBO, 0, sizeof(RenderMatrices), &mRenderData.current);

		auto playerCamera = ECSHandler::getSystem<CameraSystem>()->getCurrentCamera();
		const auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(playerCamera);
		const auto transformComp = ECSHandler::registry().pinComponent<const TransformComponent>(playerCamera);

		mRenderData.nextCameraProjection = cameraComp->getProjection();
		mRenderData.next.projection = mRenderData.nextCameraProjection.getProjectionsMatrix();
		mRenderData.next.view = transformComp->getViewMatrix();
		mRenderData.next.PV = mRenderData.next.projection * mRenderData.next.view;

		mRenderData.mNextCameraPos = transformComp->getPos(true);
		mRenderData.mNextViewDir = normalize(transformComp->getForward());
		mRenderData.mNextCamFrustum = FrustumModule::createFrustum(mRenderData.next.PV);

		mRenderData.rotate();

		renderFrame();

		prepareDataForNextFrame();
	}

	void RenderSystem::debugUpdate(float dt) {
		return;
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
		Render::BufferDesc ssbDesc;
		ssbDesc.size = sizeof(Math::Mat4) * newCap;
		ssbDesc.usage = Render::BufferUsage::Storage;
		ssbDesc.cpuAccess = Render::CpuAccess::Write;
		ssbDesc.debugName = "TransformsSSBO";
		auto newTransforms = rhi->createBuffer(ssbDesc);

		for (auto [entId, comp] : ECSHandler::registry().view<ComponentsModule::TransformMatComp>()) {
			if (static_cast<size_t>(entId) < newCap && comp) {
				rhi->updateBuffer(newTransforms, static_cast<size_t>(entId) * sizeof(Math::Mat4), sizeof(Math::Mat4), &comp->mTransform);
			}
		}

		transforms = newTransforms;
		mGpuCapacity = newCap;
		mRenderData.transformsSSBO = transforms;
		rhi->updateDescriptorSets({
			{ frameSet, 1, Render::DescriptorType::StorageBuffer, {{ transforms }} },
		});
	}

	void RenderSystem::prepareDataForNextFrame() {
		std::unordered_map<ecss::ECSType, SFE::Vector<std::pair<ecss::EntityId, uint8_t>>> dir;
		{
			FUNCTION_BENCHMARK_NAMED(dirties_copy);
			dirtiesMutex.lock();
			dir = dirties;
			for (auto& [id, entities] : dirties) {
				std::erase_if(entities, [](std::pair<ecss::EntityId, uint8_t>& val) {
					return val.second-- <= 1;
				});
			}
			dirtiesMutex.unlock();
		}

		//prepare data for next frame
		{
			FUNCTION_BENCHMARK_NAMED(copy_components_transform_armat);


			for (auto& [id, entities] : dir) {
				std::vector<ecss::EntityId> ids;
				ids.reserve(entities.size());
				for (auto [entId, _] : entities) {
					ids.push_back(entId);
				}
				std::sort(ids.begin(), ids.end());
				ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
				if (id == getDirtyId<ComponentsModule::TransformMatComp>() && !ids.empty()) {
					ensureGpuCapacity(ids.back());
				}
				auto range = OcTreeSystem::makeEntityRanges(std::move(ids));

				if (id == getDirtyId<ComponentsModule::TransformMatComp>()) {//todo support deleting
					for (auto [entId, comp] : ECSHandler::registry().view<ComponentsModule::TransformMatComp>(range)) {
						if (!comp) {
							continue;
						}
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<ComponentsModule::TransformMatComp>(entId, *comp);
						Engine::instance()->rhi()->updateBuffer(transforms, static_cast<size_t>(entId) * sizeof(Math::Mat4), sizeof(Math::Mat4), &comp->mTransform);
					}
				}
				else if (id == getDirtyId<ComponentsModule::ArmatureBonesComponent>()) {
					for (auto [entId, comp] : ECSHandler::registry().view<ComponentsModule::ArmatureBonesComponent>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<ComponentsModule::ArmatureBonesComponent>(entId, *comp);
					}
				}
				else if (id == getDirtyId<MeshComponent>()) {
					for (auto [entId, comp] : ECSHandler::registry().view<MeshComponent>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<MeshComponent>(entId, *comp);
					}
				}
				else if (id == getDirtyId<MaterialComponent>()) {
					for (auto [entId, comp] : ECSHandler::registry().view<MaterialComponent>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<MaterialComponent>(entId, *comp);
					}
				}
			}
			{
				FUNCTION_BENCHMARK_NAMED(copy_components);
				// todo i need some system which will handle dirty for render components, may be it should be render system itself, then when i updating draw registry
				ECSHandler::drawRegistry(mRenderData.currentRegistry).insert<OutlineComponent>(*ECSHandler::registry().getComponentContainer<OutlineComponent>());
				ECSHandler::drawRegistry(mRenderData.currentRegistry).insert<ComponentsModule::OccludedComponent>(*ECSHandler::registry().getComponentContainer<ComponentsModule::OccludedComponent>());
			}
		}
	}

	void RenderSystem::notify(Task task) {
		if (task.type == TRAHSFORM_RELOADED) {
			ECSHandler::registry().addComponent<ComponentsModule::TransformMatComp>(task.entity, static_cast<TransformComponent*>(task.customData)->getTransform());
			markDirty<ComponentsModule::TransformMatComp>(task.entity);
		}
		else if (task.type == ARMATURE_UPDATED) {
			markDirty<ComponentsModule::ArmatureBonesComponent>(task.entity);
		}
		else if (task.type == MATERIAL_UPDATED) {
			markDirty<ComponentsModule::MaterialComponent>(task.entity);
		}
		else if (task.type == MESH_UPDATED) {
			markDirty<ComponentsModule::MeshComponent>(task.entity);
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

		auto* engine = Engine::instance();
		if (engine->publishEditorDisplay() && !engine->presentToScreen()) {
			if (std::strcmp(name, "Debug") == 0 || std::strcmp(name, "TextOverlay") == 0) {
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
		auto cmd = rhi->beginCommandList(Render::CommandListType::Graphics);
		rhi->cmdBindDescriptorSets(cmd, {}, 0, &frameSet, 1);
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
		}
		rhi->endCommandList(cmd);

		const auto tSubmit = std::chrono::high_resolution_clock::now();
		rhi->submit(&cmd, 1);
		info->submitMs = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - tSubmit).count();
		info->gpuMs = static_cast<float>(rhi->lastGpuMs());
		if (Engine::instance()->publishEditorDisplay()) {
			rhi->publishDisplay(mRenderData.frame.framebuffer(Render::FrameId::Final));
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
		if (mGpuTimeQueries[0]) {
			//glDeleteQueries(2, mGpuTimeQueries);
		}
	}
}
