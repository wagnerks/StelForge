#include "CascadedShadowPass.h"

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"
#include "core/BoundingVolume.h"
#include "systemsModule/systems/CameraSystem.h"

#include <thread>

#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/FileSystem.h"
#include "core/ThreadPool.h"
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
	//mStatus = RenderPreparingStatus::PREPARING;
	auto& renderData = ECSHandler::getSystem<SystemsModule::RenderSystem>()->getRenderData();
	currentLock = ThreadPool::instance()->addTask<WorkerType::RENDER>([this, curPassData, entities = std::vector<unsigned>(), camProj = renderData.nextCameraProjection, view = renderData.next.view, nextFrust = renderData.mNextCamFrustum]() mutable {
		FUNCTION_BENCHMARK
		curPassData->getBatcher().drawList.clear();
		auto shadowsComp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
		if (!shadowsComp) {
			//mStatus = RenderPreparingStatus::READY;
			return;
		}

		shadowsComp->calculateLightSpaceMatrices(camProj, view);
		{
			FUNCTION_BENCHMARK_NAMED(octree);

			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			std::mutex addMtx;

			const auto& cascades = shadowsComp->cascades;
			auto aabbOctrees = octreeSys->getAABBOctrees(nextFrust.generateAABB());
			int d = 0;
			ThreadPool::instance()->addBatchTasks(aabbOctrees.size(), 5, [aabbOctrees,octreeSys, &addMtx, &cascades, &entities](size_t it) {
				if (auto treeIt = octreeSys->getOctree(aabbOctrees[it])) {
					auto lock = treeIt->readLock();
					treeIt->forEachObject([&addMtx, &cascades, &entities](const auto& obj) {
						if (std::find_if(cascades.crbegin(), cascades.crend(), [&obj](const ComponentsModule::ShadowCascade& shadow) {
							return FrustumModule::AABB::isOnFrustum(shadow.frustum, obj.pos + SFE::Math::Vec3(obj.size.x, -obj.size.y, obj.size.z) * 0.5f, obj.size);
						}) != cascades.crend()) {
							std::unique_lock lock(addMtx); 
							entities.emplace_back(obj.data.getID());
						}
					}, [&cascades](const SFE::Math::Vec3& pos, float size, auto&) {
						return std::find_if(cascades.crbegin(), cascades.crend(), [pos, size](const ComponentsModule::ShadowCascade& shadow) {
							return OcTree<ecss::EntityHandle>::isOnFrustum(shadow.frustum, pos, size);
						}) != cascades.crend();
					});
				}
				
			}).waitAll();
		}

		if (entities.empty()) {
			return;
		}
		std::ranges::sort(entities);

		{
			auto& batcher = curPassData->getBatcher();
			for (auto [ent, transform, modelComp, animComp] : ECSHandler::registry().getComponentsArray<ComponentsModule::TransformComponent, ModelComponent, ComponentsModule::AnimationComponent>(entities)) {
				if (!modelComp) {
					continue;
				}
				const auto& transformMatrix = transform->getTransform();
				for (const auto& mesh : modelComp->getModelLowestDetails().meshes) {
					batcher.addToDrawList(mesh->getVAO(), mesh->mData.vertices.size(), mesh->mData.indices.size(), mesh->mMaterial, transformMatrix, modelComp->boneMatrices, false);
				}
			}

			//batcher.sort({100.f,1300.f, 600.f});
		}

		//mStatus = RenderPreparingStatus::READY;
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
	
	ECSHandler::registry().addComponent<ComponentsModule::TransformComponent>(mShadowSource, mShadowSource.getID());
	ECSHandler::registry().addComponent<LightSourceComponent>(mShadowSource, mShadowSource.getID(), SFE::ComponentsModule::eLightType::WORLD);

	auto cmp = ECSHandler::registry().addComponent<CascadeShadowComponent>(mShadowSource, mShadowSource.getID());
	cmp->resolution = Math::Vec2{ 4096.f, 4096.f };

	auto debugData = ECSHandler::registry().addComponent<DebugDataComponent>(mShadowSource);
	debugData->stringId = "cascadeShadows";

	auto cam = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto& cameraProjection = ECSHandler::registry().getComponent<CameraComponent>(cam)->getProjection();

	cmp->shadowCascadeLevels = { cameraProjection.getNear(), 50.f, 150.f, 500.f, 5000.f };

	auto shadow = FileSystem::readJson("cascadedShadows.json");

	PropertiesModule::PropertiesSystem::deserializeProperty<CascadeShadowComponent>(mShadowSource, shadow["Properties"]);


	ECSHandler::registry().getComponent<ComponentsModule::TransformComponent>(mShadowSource)->setRotate({ -0.4f * 180.f,0.f, 0.4f * 5.f });

	initRender();
}

void CascadedShadowPass::initRender() {
	auto cmp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
	auto& cameraProjection = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection();

	cmp->updateCascades(cameraProjection);
	lightDepthMap.create3D(
		static_cast<int>(cmp->resolution.x), static_cast<int>(cmp->resolution.y),
		static_cast<int>(cmp->cascades.size()),
		GLW::DEPTH_COMPONENT32,
		GLW::DEPTH_COMPONENT,
		{
			{GLW::MIN_FILTER, GL_LINEAR},
			{GLW::MAG_FILTER, GL_LINEAR},
			{GLW::WRAP_S, GL_CLAMP_TO_EDGE},
			{GLW::WRAP_T, GL_CLAMP_TO_EDGE},
			{GLW::COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE},
			{GLW::COMPARE_FUNC, GL_LESS},
		},
		{},
		GLW::PixelDataType::FLOAT
	);	

	lightFBO.bind();
	lightFBO.addAttachmentTexture(GLW::AttachmentType::DEPTH, &lightDepthMap);
	lightFBO.setDrawBuffer(GLW::NONE);
	lightFBO.setReadBuffer(GLW::NONE);
	lightFBO.finalize();

	{
		auto guard = matricesUBO.bindWithGuard();
		matricesUBO.allocateData<Math::Mat4>(6, GLW::DYNAMIC_DRAW);
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
	if (currentLock.valid()) {
		FUNCTION_BENCHMARK_NAMED(_wait_lock);
		currentLock.wait();
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
		auto guard = matricesUBO.bindWithGuard();
		matricesUBO.setData(lightMatrices.size(), lightMatrices.data());//todo crashes when calculateLightSpaceMatrices called from another thread
	}

	GLW::ViewportStack::push({ {static_cast<int>(shadowsComp->resolution.x), static_cast<int>(shadowsComp->resolution.y)} });
	lightFBO.bind();
	GLW::clear(GLW::ColorBit::DEPTH);

	const auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();

	curPassData->getBatcher().flushAll(true);

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
