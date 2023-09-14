#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>

#include "CameraSystem.h"
#include "imgui.h"
#include "SystemsPriority.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "core/Engine.h"
#include "ecsModule/ComponentsManager.h"
#include "ecsModule/EntityBase.h"
#include "ecsModule/EntityManager.h"
#include "ecsModule/SystemManager.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "mathModule/MathUtils.h"
#include "renderModule/CascadeShadows.h"

using namespace Engine::SystemsModule;

RenderSystem::RenderSystem(RenderModule::Renderer* renderer) : mRenderer(renderer) {
	auto shadowPass = new RenderModule::RenderPasses::CascadedShadowPass();
	shadowPass->init();
	shadowPass->setPriority(CASCADE_SHADOWS);
	mRenderPasses.emplace_back(shadowPass);

	auto geometryPass = new RenderModule::RenderPasses::GeometryPass();
	geometryPass->init();
	geometryPass->setPriority(GEOMETRY);
	mRenderPasses.emplace_back(geometryPass);

	auto lightingPass = new RenderModule::RenderPasses::LightingPass();
	lightingPass->init();
	lightingPass->setPriority(LIGHTING);
	mRenderPasses.emplace_back(lightingPass);

	auto ssaoPass = new RenderModule::RenderPasses::SSAOPass();
	ssaoPass->setPriority(SSAO);
	ssaoPass->init();
	mRenderPasses.emplace_back(ssaoPass);

	std::ranges::sort(mRenderPasses);
}

void RenderSystem::preUpdate(float_t dt) {
}

void RenderSystem::update(float_t dt) {
	if (!mRenderer) {
		return;
	}

	const auto compManager = ecsModule::ECSHandler::componentManagerInstance();
	const auto entityManager = ecsModule::ECSHandler::entityManagerInstance();
	auto renderComponents = compManager->getComponentContainer<IsDrawableComponent>();
	auto playerCamera = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto& playerPos = playerCamera->getComponent<TransformComponent>()->getPos(true);

	mRenderData.mProjection = playerCamera->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix();
	mRenderData.mView = playerCamera->getComponent<TransformComponent>()->getViewMatrix();
	mRenderData.mCameraPos = playerCamera->getComponent<TransformComponent>()->getPos(true);

	static bool updateFrustum = true;
	if (updateFrustum) {
		mRenderData.mCamFrustum = FrustumModule::createFrustum(mRenderData.mProjection * mRenderData.mView);
	}

	mRenderData.mDrawableEntities.clear();
	mRenderData.mDrawableEntities.reserve(renderComponents->size());

	const auto farSqr = playerCamera->getComponent<CameraComponent>()->getProjection().getFar() * playerCamera->getComponent<CameraComponent>()->getProjection().getFar();

	for (const auto& renderComp : *renderComponents) {
		if (Math::distanceSqr(playerPos, compManager->getComponent<TransformComponent>(renderComp.getOwnerId())->getPos(true)) > farSqr) {
			continue;
		}

		mRenderData.mDrawableEntities.emplace_back(renderComp.getOwnerId());
	}

	for (const auto renderPass : mRenderPasses) {
		renderPass->render(mRenderer, mRenderData);
	}

	auto curCamera = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	CascadeShadows::debugDraw(mRenderData.mCascadedShadowsPassData.shadows->getCacheLightSpaceMatrices(), curCamera->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix(), curCamera->getComponent<TransformComponent>()->getViewMatrix());

	ImGui::Begin("render mode");
	ImGui::Checkbox("update cam frustum", &updateFrustum);
	ImGui::Checkbox("wireframe", &mRenderData.mWireframeMode);
	ImGui::End();

	if (ImGui::Begin("geometry pass data")) {
		float size = 500.f;
		ImGui::Image((void*)static_cast<size_t>(mRenderData.mGeometryPassData.gAlbedoSpec), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
		ImGui::Image((void*)static_cast<size_t>(mRenderData.mGeometryPassData.gPosition), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
		ImGui::Image((void*)static_cast<size_t>(mRenderData.mGeometryPassData.gNormal), { size,size }, { 0.f, 1.f }, { 1.f,0.f });

		ImGui::Image((void*)static_cast<size_t>(mRenderData.mSSAOPassData.mSsaoColorBuffer), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
		ImGui::Image((void*)static_cast<size_t>(mRenderData.mSSAOPassData.mSsaoColorBufferBlur), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
	}



	ImGui::End();
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
