#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>

#include "imgui.h"
#include "SystemsPriority.h"
#include "componentsModule/RenderComponent.h"
#include "core/Engine.h"
#include "ecsModule/ComponentsManager.h"
#include "ecsModule/EntityBase.h"
#include "ecsModule/EntityManager.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "mathModule/MathUtils.h"

using namespace GameEngine::SystemsModule;

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
	lightingPass->setPriority(LIGHTING);
	mRenderPasses.emplace_back(lightingPass);

	/*auto ssaoPass = new RenderModule::RenderPasses::SSAOPass();
	ssaoPass->setPriority(SSAO);
	ssaoPass->init();
	mRenderPasses.emplace_back(ssaoPass);*/

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
	auto renderComponents = compManager->getComponentContainer<RenderComponent>();
	auto playerCamera = Engine::getInstance()->getCamera(); //todo entity player should have camera component
	auto playerPos = playerCamera->getComponent<TransformComponent>()->getPos(true);
	mRenderData.projection = playerCamera->getProjectionsMatrix();
	mRenderData.view = playerCamera->getComponent<TransformComponent>()->getViewMatrix();
	mRenderData.cameraPos = playerCamera->getComponent<TransformComponent>()->getPos(true);

	ImGui::Begin("keklol");
	static bool updateFrustum = true;
	ImGui::Checkbox("update cam frustum", &updateFrustum);
	ImGui::End();

	if (updateFrustum) {
		mRenderData.camFrustum = FrustumModule::createFrustum(mRenderData.projection * mRenderData.view);
	}

	mRenderData.mDrawableEntities.clear();
	mRenderData.mDrawableEntities.reserve(renderComponents->size());
	for (const auto& renderComp : *renderComponents) {
		if (renderComp.isDrawable()) {
			if (auto transform = compManager->getComponent<TransformComponent>(renderComp.getOwnerId())) {
				if (GameEngine::Math::distanceSqr(playerPos, transform->getPos(true)) > playerCamera->cameraView.getZFar()*playerCamera->cameraView.getZFar()) {
					continue;
				}
			}

			mRenderData.mDrawableEntities.push_back(renderComp.getOwnerId());
		}
	}

	for (const auto renderPass : mRenderPasses) {
		renderPass->render(mRenderer, mRenderData);
	}

	mRenderData.mCascadedShadowsPassData.shadows->debugDraw();

	ImGui::Begin("render mode");

	if (ImGui::Button("wireframe")){
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	}

	if (ImGui::Button("usual")){
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	ImGui::End();

	ImGui::Begin("kek");
	float size = 500.f;
	ImGui::Image((void*)static_cast<size_t>(mRenderData.mGeometryPassData.gAlbedoSpec), {size,size}, {0.f, 1.f}, {1.f,0.f});
	ImGui::Image((void*)static_cast<size_t>(mRenderData.mGeometryPassData.gPosition), {size,size}, {0.f, 1.f}, {1.f,0.f});
	ImGui::Image((void*)static_cast<size_t>(mRenderData.mGeometryPassData.gNormal), {size,size}, {0.f, 1.f}, {1.f,0.f});

	ImGui::Image((void*)static_cast<size_t>(mRenderData.mSSAOPassData.mSsaoColorBuffer), {size,size}, {0.f, 1.f}, {1.f,0.f});
	ImGui::Image((void*)static_cast<size_t>(mRenderData.mSSAOPassData.mSsaoColorBufferBlur), {size,size}, {0.f, 1.f}, {1.f,0.f});

	ImGui::End();
}

void RenderSystem::postUpdate(float_t dt) {

}

const std::vector<GameEngine::RenderModule::RenderPass*>& RenderSystem::getRenderPasses() {
	return mRenderPasses;
}