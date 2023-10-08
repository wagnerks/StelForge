#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>

#include "CameraSystem.h"
#include "imgui.h"
#include "SystemManager.h"
#include "SystemsPriority.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "..\ecss\Registry.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "mathModule/MathUtils.h"
#include "renderModule/CascadeShadows.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/ShadersPass.h"

using namespace Engine::SystemsModule;

void RenderSystem::setRenderType(RenderMode type) {
	mRenderData.mRenderType = type;
}

RenderSystem::RenderSystem(RenderModule::Renderer* renderer) : mRenderer(renderer) {
	auto shadowPass = new RenderModule::RenderPasses::CascadedShadowPass();
	shadowPass->init();
	shadowPass->setPriority(CASCADE_SHADOWS);
	mRenderPasses.emplace_back(shadowPass);

	auto pointLightPass = new RenderModule::RenderPasses::PointLightPass();
	pointLightPass->initRender();
	pointLightPass->setPriority(CASCADE_SHADOWS);
	mRenderPasses.emplace_back(pointLightPass);

	auto geometryPass = new RenderModule::RenderPasses::GeometryPass();
	geometryPass->init();
	geometryPass->setPriority(GEOMETRY);
	mRenderPasses.emplace_back(geometryPass);

	auto shadersPass = new RenderModule::RenderPasses::ShadersPass();
	shadersPass->setPriority(SHADERS);
	mRenderPasses.emplace_back(shadersPass);

	auto lightingPass = new RenderModule::RenderPasses::LightingPass();
	lightingPass->init();
	lightingPass->setPriority(LIGHTING);
	mRenderPasses.emplace_back(lightingPass);

	auto ssaoPass = new RenderModule::RenderPasses::SSAOPass();
	ssaoPass->setPriority(SSAO);
	ssaoPass->init();
	mRenderPasses.emplace_back(ssaoPass);

	std::sort(mRenderPasses.begin(), mRenderPasses.end(), [](RenderModule::RenderPass* a, RenderModule::RenderPass* b) {
		return a->getPriority() < b->getPriority();
	});
}

void RenderSystem::preUpdate(float_t dt) {
}

void RenderSystem::update(float_t dt) {
	if (!mRenderer) {
		return;
	}

	const auto compManager = ECSHandler::registry();

	auto& playerCamera = ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();

	auto& playerPos = compManager->getComponent<TransformComponent>(playerCamera)->getPos(true);

	mRenderData.mProjection = compManager->getComponent<CameraComponent>(playerCamera)->getProjection().getProjectionsMatrix();
	mRenderData.mView = compManager->getComponent<TransformComponent>(playerCamera)->getViewMatrix();
	mRenderData.mCameraPos = compManager->getComponent<TransformComponent>(playerCamera)->getPos(true);

	mRenderData.mCamFrustum = FrustumModule::createFrustum(mRenderData.mProjection * mRenderData.mView);

	const auto farSqr = compManager->getComponent<CameraComponent>(playerCamera)->getProjection().getFar() * compManager->getComponent<CameraComponent>(playerCamera)->getProjection().getFar();
	/*for (auto [isDrawable, transform] : ecss::Registry::createComponentsSelector<IsDrawableComponent, TransformComponent>()) {
		if (!&isDrawable || !&transform) {
			continue;
		}
		if (Math::distanceSqr(playerPos, transform.getPos(true)) > farSqr) {
			continue;
		}

		mRenderData.mDrawableEntities.emplace_back(isDrawable.getEntityId());
	}*/

	for (const auto renderPass : mRenderPasses) {
		renderPass->render(mRenderer, mRenderData);
	}

	auto curCamera = ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	CascadeShadowComponent::debugDraw(compManager->getComponent<CascadeShadowComponent>(mRenderData.mCascadedShadowsPassData.shadows)->getCacheLightSpaceMatrices(), compManager->getComponent<CameraComponent>(curCamera)->getProjection().getProjectionsMatrix(), compManager->getComponent<TransformComponent>(curCamera)->getViewMatrix());

	{
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Render mode")) {
				if (ImGui::RadioButton("default", mRenderData.mRenderType == RenderMode::DEFAULT)) {
					mRenderData.mRenderType = RenderMode::DEFAULT;
				}
				if (ImGui::RadioButton("wireframe", mRenderData.mRenderType == RenderMode::WIREFRAME)) {
					mRenderData.mRenderType = RenderMode::WIREFRAME;
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Debug")) {
				if (ImGui::BeginMenu("Shadows debug")) {
					ImGui::Checkbox("shadows raw data show", &mShadowsDebugDataDraw);
					ImGui::EndMenu();
				}
				ImGui::Checkbox("geometry pass data", &mGeometryPassDataWindow);
				ImGui::EndMenu();
			}
		}
		ImGui::EndMainMenuBar();
	}


	if (mGeometryPassDataWindow) {
		if (ImGui::Begin("geometry pass result", &mGeometryPassDataWindow)) {
			float size = ImGui::GetWindowContentRegionWidth();
			if (ImGui::TreeNode("gAlbedoSpec")) {
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mGeometryPassData.gAlbedoSpec)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("gPosition")) {
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mGeometryPassData.gPosition)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("gNormal")) {
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mGeometryPassData.gNormal)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("gOutlines")) {
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mGeometryPassData.gOutlines)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("gLights")) {
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mGeometryPassData.gLights)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("ssao")) {
				ImGui::Text("mSsaoColorBuffer");
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mSSAOPassData.mSsaoColorBuffer)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });

				ImGui::Text("mSsaoColorBufferBlur");
				ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(mRenderData.mSSAOPassData.mSsaoColorBufferBlur)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	if (!mRenderData.mCascadedShadowsPassData.shadowCascadeLevels.empty() && mShadowsDebugDataDraw) {
		auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
		sh->use();
		sh->setInt("depthMap", 31);

		auto a = 250.f / RenderModule::Renderer::SCR_WIDTH;
		auto b = 250.f / RenderModule::Renderer::SCR_HEIGHT;

		for (auto i = 0; i < mRenderData.mCascadedShadowsPassData.shadowCascadeLevels.size() - 1; i++) {
			sh->setFloat("near_plane", mRenderData.mCascadedShadowsPassData.shadowCascadeLevels[i]);
			sh->setFloat("far_plane", mRenderData.mCascadedShadowsPassData.shadowCascadeLevels[i + 1]);
			sh->setInt("layer", i);

			RenderModule::Utils::renderQuad(1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
		}
	}

	//auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
	//sh->use();
	//sh->setInt("depthMap", 30);

	//auto a = 250.f / RenderModule::Renderer::SCR_WIDTH;
	//auto b = 250.f / RenderModule::Renderer::SCR_HEIGHT;

	//for (auto i = 0; i < 6; i++) {
	//	sh->setFloat("near_plane", 0.1f);
	//	sh->setFloat("far_plane", 100.f);
	//	sh->setInt("layer", i);

	//	RenderModule::Utils::renderQuad(1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
	//}
}

void RenderSystem::postUpdate(float_t dt) {
}

const std::vector<Engine::RenderModule::RenderPass*>& RenderSystem::getRenderPasses() {
	return mRenderPasses;
}

RenderSystem::~RenderSystem() {
	for (auto renderPass : mRenderPasses) {
		delete renderPass;
	}
}
