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
#include "logsModule/logger.h"
#include "mathModule/Forward.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::RenderModule::RenderPasses;

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
			for (auto [ent, transform, modelComp] : ECSHandler::registry().getComponentsArray<ComponentsModule::TransformComponent, ModelComponent>(entities)) {
				if (!modelComp) {
					continue;
				}

				const auto& transformMatrix = transform->getTransform();
				for (const auto& mesh : modelComp->getModelLowestDetails().mMeshHandles) {
					batcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transformMatrix, false);
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
	freeBuffers();
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
	freeBuffers();

	auto cmp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
	auto& cameraProjection = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection();

	cmp->updateCascades(cameraProjection);

	glGenFramebuffers(1, &lightFBO);

	glGenTextures(1, &lightDepthMaps);
	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
	glTexImage3D(
		GL_TEXTURE_2D_ARRAY,
		0,
		GL_DEPTH_COMPONENT32,
		static_cast<int>(cmp->resolution.x),
		static_cast<int>(cmp->resolution.y),
		static_cast<int>(cmp->cascades.size()),
		0,
		GL_DEPTH_COMPONENT,
		GL_FLOAT,
		nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		SFE::LogsModule::Logger::LOG_ERROR("FRAMEBUFFER::CascadeShadow Framebuffer is not complete!");
	}

	glGenBuffers(1, &matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(Math::Mat4) * 6, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadedShadowPass::freeBuffers() const {
	glDeleteFramebuffers(1, &lightFBO);
	glDeleteTextures(1, &lightDepthMaps);
	glDeleteBuffers(1, &matricesUBO);
}

void CascadedShadowPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
	if (!mInited || !renderer) {
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

	if (!lightMatrices.empty()) {
		glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Math::Mat4) * lightMatrices.size(), &lightMatrices[0]);//todo crashes when calculateLightSpaceMatrices called from another thread
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, lightDepthMaps, 0);

	glViewport(0, 0, static_cast<int>(shadowsComp->resolution.x), static_cast<int>(shadowsComp->resolution.y));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	const auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();

	curPassData->getBatcher().flushAll(true);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, SFE::RenderModule::Renderer::SCR_RENDER_W, SFE::RenderModule::Renderer::SCR_RENDER_H);

	updateRenderData(renderDataHandle);
}

void CascadedShadowPass::updateRenderData(SystemsModule::RenderData& renderDataHandle) const {
	auto shadowsComp = ECSHandler::registry().getComponent<CascadeShadowComponent>(mShadowSource);
	renderDataHandle.mCascadedShadowsPassData.shadowMapTexture = lightDepthMaps;
	renderDataHandle.mCascadedShadowsPassData.lightDirection = ECSHandler::registry().getComponent<TransformComponent>(mShadowSource)->getForward();
	renderDataHandle.mCascadedShadowsPassData.lightColor = ECSHandler::registry().getComponent<LightSourceComponent>(mShadowSource)->getLightColor();
	renderDataHandle.mCascadedShadowsPassData.resolution = shadowsComp->resolution;
	renderDataHandle.mCascadedShadowsPassData.cameraFarPlane = shadowsComp->cascades.back().viewProjection.getFar();
	renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels = shadowsComp->shadowCascadeLevels;
	renderDataHandle.mCascadedShadowsPassData.shadows = mShadowSource;
	renderDataHandle.mCascadedShadowsPassData.shadowCascades = shadowsComp->cascades;
	renderDataHandle.mCascadedShadowsPassData.shadowsIntensity = shadowsComp->shadowIntensity;
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
