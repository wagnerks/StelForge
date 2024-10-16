#include "CascadedShadowPass.h"

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "systemsModule/systems/CameraSystem.h"

#include <thread>

#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/FileSystem.h"
#include "multithreading/ThreadPool.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "glWrapper/ViewportStack.h"
#include "logsModule/logger.h"
#include "mathModule/Forward.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;

void CascadedShadowPass::prepare() {
	if (!ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource)) {
		return;
	}

	auto curPassData = getContainer().getCurrentPassData();
	curPassData->mStatus = RenderPreparingStatus::PREPARING;
	auto& renderData = ECSHandler::getSystem<SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, camProj = renderData.nextCameraProjection, view = renderData.next.view]() mutable {
		FUNCTION_BENCHMARK;

		curPassData->getBatcher().clear();
		auto shadowsComp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
		if (!shadowsComp) {
			curPassData->mStatus = RenderPreparingStatus::READY;
			return;
		}

		shadowsComp->calculateLightSpaceMatrices(camProj, view);

		SFE::Vector<ecss::EntityId> entities;
		{
			FUNCTION_BENCHMARK_NAMED(octree);

			const auto& cascades = shadowsComp->cascades;

			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			for (auto& cascade : cascades) {
				for (auto& treePos : octreeSys->getAABBOctrees(cascade.frustum.generateAABB())) {
					if (const auto tree = octreeSys->getOctree(treePos)) {
						auto lock = tree->readLock();
						tree->forEachObjectInFrustum(cascade.frustum, [&entities, &cascade](const auto& obj, bool entirely) {
							if (entirely || FrustumModule::AABB::isOnFrustum(cascade.frustum, obj.pos, obj.size)) {
								entities.emplace_back(obj.data);
							}
						});
					}
				}
			}
		}

		if (entities.empty()) {
			curPassData->mStatus = RenderPreparingStatus::READY;
			return;
		}
		entities.sort();
		entities.removeDublicatesSorted();
		{
			auto& batcher = curPassData->getBatcher();
			{
				FUNCTION_BENCHMARK_NAMED(addedToBatcher)
					
				for (auto [ent, transform, meshComp, oclComp] : ECSHandler::drawRegistry(nextRegistry).forEach<const ComponentsModule::TransformMatComp, const MeshComponent, const ComponentsModule::OccludedComponent>({ entities }, false)) {
					if (!meshComp) {
						continue;
					}
					if (oclComp && oclComp->occluded) {
						continue;
					}

					for (const auto& mesh : meshComp->meshGraph) {
						batcher.addToDrawList(ent, mesh.value.vaoId, mesh.value.verticesCount, mesh.value.indicesCount, {}, transform->mTransform);
					}

				}
			}
			FUNCTION_BENCHMARK_NAMED(sort)
			batcher.sort({100.f,1300.f, 600.f});//todo
		}
		curPassData->mStatus = RenderPreparingStatus::READY;
	});
}

CascadedShadowPass::CascadedShadowPass() {
	getContainer().init(2);
}

CascadedShadowPass::~CascadedShadowPass() {

}

void CascadedShadowPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mShadowSource = ECSHandler::registry().takeEntity();
	
	auto transform = ECSHandler::registry().addComponent<TransformComponent>(mShadowSource, mShadowSource);
	ECSHandler::registry().addComponent<LightSourceComponent>(mShadowSource, mShadowSource, SFE::ComponentsModule::eLightType::WORLD);

	auto cmp = ECSHandler::registry().addComponent<CascadeShadowComponent>(mShadowSource, mShadowSource);
	

	auto debugData = ECSHandler::registry().addComponent<DebugDataComponent>(mShadowSource);
	debugData->stringId = "cascadeShadows";

	auto shadow = FileSystem::readJson("cascadedShadows.json");

	PropertiesModule::PropertiesSystem::deserializeProperty<CascadeShadowComponent>(mShadowSource, shadow["Properties"]);
	//cmp->resolution = Math::Vec2{ 256.f, 256.f };
	//cmp->shadowCascadeLevels = { cameraProjection.getNear(), 50.f, 150.f, /*500.f, 5000.f*/ };
	//cmp->updateCascades(cameraProjection);
	transform->setRotate({ -0.4f * 180.f,0.f, 0.4f * 5.f });

	initRender();
}

void CascadedShadowPass::initRender() {
	auto cmp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
	auto& cameraProjection = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection();

	cmp->updateCascades(cameraProjection);
	lightDepthMap.parameters.minFilter = GLW::TextureMinFilter::LINEAR;
	lightDepthMap.parameters.magFilter = GLW::TextureMagFilter::LINEAR;
	lightDepthMap.parameters.wrap.S = GLW::TextureWrap::CLAMP_TO_EDGE;
	lightDepthMap.parameters.wrap.T = GLW::TextureWrap::CLAMP_TO_EDGE;
	lightDepthMap.parameters.compareMode = GLW::TextureCompareMode::COMPARE_REF_TO_TEXTURE;
	lightDepthMap.parameters.compareFunc = GLW::CompareFunc::LESS;
	lightDepthMap.width = static_cast<int>(cmp->resolution.x);
	lightDepthMap.height = static_cast<int>(cmp->resolution.y);
	lightDepthMap.depth = static_cast<int>(cmp->cascades.size());
	lightDepthMap.pixelFormat = GLW::DEPTH_COMPONENT32;
	lightDepthMap.textureFormat = GLW::DEPTH_COMPONENT;
	lightDepthMap.pixelType = GLW::FLOAT;

	lightDepthMap.create3D();	

	lightFBO.bind();
	lightFBO.addAttachmentTexture(GLW::AttachmentType::DEPTH, &lightDepthMap);
	lightFBO.setDrawBuffer(GLW::NONE);
	lightFBO.setReadBuffer(GLW::NONE);
	lightFBO.finalize();

	{
		matricesUBO.generate();
		auto guard = matricesUBO.lock();
		matricesUBO.reserve(6);
		matricesUBO.setBufferBinding(0);
	}

	GLW::Framebuffer::bindDefaultFramebuffer();
}


void CascadedShadowPass::render(SystemsModule::RenderData& renderDataHandle) {
	if (!mInited) {
		return;
	}

	debug(renderDataHandle);

	FUNCTION_BENCHMARK;

	updateRenderData(renderDataHandle);
	{
		FUNCTION_BENCHMARK_NAMED(_wait_lock);
		const auto curPassData = getContainer().getCurrentPassData();
		while(curPassData->mStatus != RenderPreparingStatus::READY) {
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	auto shadowsComp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
	if (!shadowsComp) {
		return;
	}
	
	const auto lightMatrices = shadowsComp->getLightSpaceMatrices();//todo copy, because of crash below

	const auto curPassData = getContainer().getCurrentPassData();
	getContainer().rotate();
	prepare();

	updateRenderData(renderDataHandle);

	if (!lightMatrices.empty()) {
		FUNCTION_BENCHMARK_NAMED(_bind_ubo);
		auto guard = matricesUBO.lock();
		matricesUBO.setData(lightMatrices);//todo crashes when calculateLightSpaceMatrices called from another thread
	}

	GLW::ViewportStack::push({ {static_cast<int>(shadowsComp->resolution.x), static_cast<int>(shadowsComp->resolution.y)} });
	lightFBO.bind();
	GLW::clear(GLW::ColorBit::DEPTH);

	const auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();
	{
		FUNCTION_BENCHMARK_NAMED(_flush);
		curPassData->getBatcher().flushAll();
	}

	GLW::Framebuffer::bindDefaultFramebuffer();
	GLW::ViewportStack::pop();
}

void CascadedShadowPass::updateRenderData(SystemsModule::RenderData& renderDataHandle) {
	auto shadowsComp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
	renderDataHandle.mCascadedShadowsPassData = &mData;

	renderDataHandle.mCascadedShadowsPassData->shadowMapTexture = lightDepthMap.mId;
	renderDataHandle.mCascadedShadowsPassData->lightDirection = ECSHandler::registry().getComponent<TransformComponent>(mShadowSource)->getForward();
	renderDataHandle.mCascadedShadowsPassData->lightColor = ECSHandler::registry().getComponent<LightSourceComponent>(mShadowSource)->getLightColor();
	renderDataHandle.mCascadedShadowsPassData->resolution = shadowsComp->resolution;
	renderDataHandle.mCascadedShadowsPassData->cameraFarPlane = shadowsComp->cascades.back().viewProjection.getFar();
	renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels = shadowsComp->shadowCascadeLevels;
	renderDataHandle.mCascadedShadowsPassData->shadows = mShadowSource;
	renderDataHandle.mCascadedShadowsPassData->shadowCascades = shadowsComp->cascades;
	renderDataHandle.mCascadedShadowsPassData->shadowsIntensity = shadowsComp->shadowIntensity;
}

void CascadedShadowPass::debug(SystemsModule::RenderData& renderDataHandle) {
	return;
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			if (ImGui::BeginMenu("Shadows debug")) {

				if (mShadowSource) {
					if (ImGui::Button("cache")) {
						ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource)->cacheMatrices();
					}

					if (ImGui::Button("clear")) {
						ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource)->clearCacheMatrices();
					}
					static float sunProgress = 0.4f;
					if (ImGui::DragFloat("sun pos", &sunProgress, 0.001f, 0.f)) {
						/*auto x = cosf(Math::radians(-sunProgress * 180.f));
						auto y = sinf(Math::radians(sunProgress * 180.f));
						auto z = sinf(Math::radians(sunProgress * 180.f));*/
						ECSHandler::registry().getComponent<TransformComponent>(mShadowSource)->setRotate({ -sunProgress * 180.f,0.f, sunProgress * 5.f });
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();
}
