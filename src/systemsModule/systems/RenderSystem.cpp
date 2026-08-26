#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>

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

#include "componentsModule/TransformComponent.h"
#include "renderModule/TextRenderer.h"
#include "renderModule/renderPasses/GUIPass.h"
#include <debugModule/SystemLoadHelper.h>

#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "containersModule/Vector.h"
#include "renderModule/renderPasses/FinalPass.h"

namespace SFE::SystemsModule {

	template <typename PassType>
	void RenderSystem::addRenderPass() {
		size_t i = 0;
		for (; i < RENDER_PASSES_PRIORITY.size(); i++) {
			if (RENDER_PASSES_PRIORITY[i] == typeid(PassType).hash_code()) {
				break;
			}
		}

		assert(i != RENDER_PASSES_PRIORITY.size());
		if (i == RENDER_PASSES_PRIORITY.size()) {
			return;
		}

		auto pass = new PassType();
		pass->init(mRenderData);
		pass->setPriority(i);

		mRenderPasses.emplace_back(pass);

		std::sort(mRenderPasses.begin(), mRenderPasses.end(), [](Render::RenderPass* a, Render::RenderPass* b) {
			return a->getPriority() < b->getPriority();
		});
	}

	RenderSystem::RenderSystem() : System({ SFE::SystemsModule::TaskType::TRAHSFORM_RELOADED , SFE::SystemsModule::TaskType::ARMATURE_UPDATED, MATERIAL_UPDATED, MESH_UPDATED }) {
		mRenderPasses.reserve(RENDER_PASSES_PRIORITY.size());

		//addRenderPass<Render::RenderPasses::OcclusionPass>();
		addRenderPass<Render::RenderPasses::CascadedShadowPass>();//todo passes shoudle be created according to settings
		//addRenderPass<Render::RenderPasses::PointLightPass>();
		addRenderPass<Render::RenderPasses::GeometryPass>();
		addRenderPass<Render::RenderPasses::ShadersPass>();
		addRenderPass<Render::RenderPasses::LightingPass>();
		addRenderPass<Render::RenderPasses::SSAOPass>();
		//addRenderPass<Render::RenderPasses::DebugPass>();
		//addRenderPass<Render::RenderPasses::GUIPass>();
		addRenderPass<Render::RenderPasses::FinalPass>();
	
		{
			Render::BufferDesc uboDesc;
			uboDesc.size = sizeof(RenderMatrices);
			uboDesc.usage = Render::BufferUsage::Uniform;
			uboDesc.cpuAccess = Render::CpuAccess::Write;
			uboDesc.debugName = "CameraMatricesUBO";
			cameraMatricesUBO = Engine::instance()->rhi()->createBuffer(uboDesc);
		}

		constexpr size_t maxDrawSize = 100;
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
				{ 2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex },
			};
			frameSetLayout = Engine::instance()->rhi()->createDescriptorSetLayout(frameLayout);
			frameSet = Engine::instance()->rhi()->allocateDescriptorSet(frameSetLayout);
			Engine::instance()->rhi()->updateDescriptorSets({
				{ frameSet, 5, Render::DescriptorType::UniformBuffer, {{ cameraMatricesUBO }} },
				{ frameSet, 1, Render::DescriptorType::StorageBuffer, {{ transforms }} },
				{ frameSet, 2, Render::DescriptorType::StorageBuffer, {{ bones }} },
			});
		}

		ECSHandler::drawRegistry(0).registerArray<ComponentsModule::TransformMatComp, MeshComponent, MaterialComponent, ComponentsModule::ArmatureBonesComponent>(0);
		ECSHandler::drawRegistry(1).registerArray<ComponentsModule::TransformMatComp, MeshComponent, MaterialComponent, ComponentsModule::ArmatureBonesComponent>(0);

		// init gpu timer queries
		//glGenQueries(2, mGpuTimeQueries);
	}

	void RenderSystem::update(float_t dt) {
		FUNCTION_BENCHMARK;

		mRenderData.current = mRenderData.next;
		mRenderData.cameraProjection = mRenderData.nextCameraProjection;

		mRenderData.mCameraPos = mRenderData.mNextCameraPos;
		mRenderData.mViewDir = mRenderData.mNextViewDir;
		mRenderData.mCamFrustum = mRenderData.mNextCamFrustum;

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
		mRenderData.mNextCamFrustum = cameraComp->getFrustum();

		mRenderData.rotate();

		renderFrame();

		prepareDataForNextFrame();
	}

	void RenderSystem::debugUpdate(float dt) {
		return;
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
				ecss::Ranges<ecss::EntityId> range;
				for (auto [entId, _] : entities) {
					range.insert(entId);
				}

				if (id == getDirtyId<ComponentsModule::TransformMatComp>()) {//todo support deleting
					for (auto [entId, comp] : ECSHandler::registry().view<ComponentsModule::TransformMatComp>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<ComponentsModule::TransformMatComp>(entId, *comp);
						Engine::instance()->rhi()->updateBuffer(transforms, entId * sizeof(Math::Mat4), sizeof(Math::Mat4), &comp->mTransform);
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

	void RenderSystem::renderFrame()
	{
		mCpuFrameStart = std::chrono::high_resolution_clock::now();

		// swap gpu queries (we measure frame N with query A while retrieving result of frame N-1 from query B)
		int prevQuery = mCurrentGpuQuery;
		mCurrentGpuQuery = (mCurrentGpuQuery + 1) % 2;

		// begin new GPU timing for this frame
		//glBeginQuery(GL_TIME_ELAPSED, mGpuTimeQueries[mCurrentGpuQuery]);

		auto rhi = Engine::instance()->rhi();
		auto cmd = rhi->beginCommandList(Render::CommandListType::Graphics);
		rhi->cmdBindDescriptorSets(cmd, {}, 0, &frameSet, 1);
		for (const auto renderPass : mRenderPasses) {
			renderPass->render(mRenderData, cmd);
		}
		rhi->endCommandList(cmd);

		rhi->submit(&cmd, 1);
		rhi->present();

		// end gpu query for current frame
		//glEndQuery(GL_TIME_ELAPSED);

		//{
		//	// try retrieve previous frame gpu timing (non-blocking if available)
		//	GLuint available = 0;
		//	glGetQueryObjectuiv(mGpuTimeQueries[prevQuery], GL_QUERY_RESULT_AVAILABLE, &available);
		//	if (available) {
		//		unsigned long long timeElapsed = 0; // nanoseconds
		//		glGetQueryObjectui64v(mGpuTimeQueries[prevQuery], GL_QUERY_RESULT, &timeElapsed);
		//		mLastGpuTimeMs = double(timeElapsed) / 1'000'000.0; // ns -> ms
		//	}

		//	static Debug::SystemLoadWin sys;
		//	static bool inited = false;
		//	if (!inited) { sys.init(); inited = true; }

		//	sys.update(dt);

		//	auto font = Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20);
		//	auto red = Math::Vec3{ 1.f,0.f,0.f };
		//	auto green = Math::Vec3{ 0.f,1.f,0.f };
		//	
		//	Render::TextRenderer::instance()->renderText(
		//		"CPU load: " + std::to_string((int)std::round(sys.cpuPercentUI())) + "%", 10.f, 110.f, 1.f, red, font);

		//	Render::TextRenderer::instance()->renderText(
		//		sys.gpu() >= 0
		//		? "GPU load: " + std::to_string((int)std::round(sys.gpuPercentUI())) + "%"
		//		: "GPU load: n/a",
		//		10.f, 140.f, 1.f, green, font);

		//	// CPU frame end
		//	auto cpuEnd = std::chrono::high_resolution_clock::now();
		//	mLastCpuFrameMs = std::chrono::duration<double, std::milli>(cpuEnd - mCpuFrameStart).count();

		//	// compute % load relative to frame time (dt). dt is in seconds.
		//	double frameMs = max(0.000001, static_cast<double>(Engine::instance()->getDeltaTime()) * 1000.0);
		//	double cpuLoad = std::clamp(mLastCpuFrameMs / frameMs * 100.0, 0.0, 999.9); // allow >100 if over budget but clamp display
		//	double gpuLoad = std::clamp(mLastGpuTimeMs / frameMs * 100.0, 0.0, 999.9);
		//	typedef int (WINAPI* PFNWGLGETSWAPINTERVALEXTPROC) ();
		//	static PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT = nullptr;

		//
		//	if (!wglGetSwapIntervalEXT) {
		//		wglGetSwapIntervalEXT = reinterpret_cast<PFNWGLGETSWAPINTERVALEXTPROC>(
		//			wglGetProcAddress("wglGetSwapIntervalEXT"));
		//	}
		//	auto interval = wglGetSwapIntervalEXT ? wglGetSwapIntervalEXT() : -1; // -1 = узнать не удалось
		//
		//
		//	// overlay
		//	Render::TextRenderer::instance()->renderText("FPS: " + std::to_string(Engine::instance()->getFPS()), 10.f, 50.f, 1.f, Math::Vec3{1.f, 0.f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("dt: " + std::to_string(Engine::instance()->getDeltaTime()), 10.f, 80.f, 1.f, Math::Vec3{1.f, 0.f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("CPU(ms): " + std::to_string(mLastCpuFrameMs), 10.f, 170.f, 1.f, Math::Vec3{1.f, 0.8f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("GPU(ms): " + std::to_string(mLastGpuTimeMs), 10.f, 200.f, 1.f, Math::Vec3{ 0.2f, 1.f, 0.2f }, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("VSYNC: " + std::to_string(interval), 10.f, 230.f, 1.f, Math::Vec3{ 0.2f, 1.f, 0.2f }, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("objects rendered: " + std::to_string(RenderDebugInfo::instance()->objectsRendered), 10.f, 260.f, 1.f, Math::Vec3{ 0.2f, 1.f, 0.2f }, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("vertices rendered: " + std::to_string(RenderDebugInfo::instance()->verticesRendered), 10.f, 290.f, 1.f, Math::Vec3{ 0.2f, 1.f, 0.2f }, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		//	Render::TextRenderer::instance()->renderText("draw calls: " + std::to_string(RenderDebugInfo::instance()->drawCalls), 10.f, 320.f, 1.f, Math::Vec3{0.2f, 1.f, 0.2f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));

		//	RenderDebugInfo::instance()->objectsRendered = 0;
		//	RenderDebugInfo::instance()->verticesRendered = 0;
		//	RenderDebugInfo::instance()->drawCalls = 0;
		//}
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
