#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>

#include "CameraSystem.h"
#include "imgui.h"
#include "OcTreeSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "renderModule/renderPasses/DebugPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/ShadersPass.h"

#include "componentsModule/TransformComponent.h"
#include "renderModule/TextRenderer.h"
#include "renderModule/renderPasses/GUIPass.h"

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
		pass->init();
		pass->setPriority(i);

		mRenderPasses.emplace_back(pass);

		std::sort(mRenderPasses.begin(), mRenderPasses.end(), [](Render::RenderPass* a, Render::RenderPass* b) {
			return a->getPriority() < b->getPriority();
		});
	}

	RenderSystem::RenderSystem(){
		mRenderPasses.reserve(RENDER_PASSES_PRIORITY.size());

		addRenderPass<Render::RenderPasses::CascadedShadowPass>();
		addRenderPass<Render::RenderPasses::PointLightPass>();
		addRenderPass<Render::RenderPasses::GeometryPass>();
		addRenderPass<Render::RenderPasses::ShadersPass>();
		addRenderPass<Render::RenderPasses::LightingPass>();
		addRenderPass<Render::RenderPasses::SSAOPass>();
		addRenderPass<Render::RenderPasses::DebugPass>();
		addRenderPass<Render::RenderPasses::GUIPass>();

	
		auto guard = cameraMatricesUBO.bindWithGuard();
		cameraMatricesUBO.allocateData<RenderMatrices>(1, GLW::DYNAMIC_DRAW);
		cameraMatricesUBO.setBufferBinding(5);
	}

	void RenderSystem::update(float_t dt) {
		FUNCTION_BENCHMARK;

		mRenderData.current = mRenderData.next;
		mRenderData.cameraProjection = mRenderData.nextCameraProjection;

		mRenderData.mCameraPos = mRenderData.mNextCameraPos;
		mRenderData.mViewDir = mRenderData.mNextViewDir;
		mRenderData.mCamFrustum = mRenderData.mNextCamFrustum;

		{
			auto guard = cameraMatricesUBO.bindWithGuard();
			cameraMatricesUBO.setData(1, &mRenderData.current);
		}

		auto& playerCamera = ECSHandler::getSystem<CameraSystem>()->getCurrentCamera();
		const auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(playerCamera);
		const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(playerCamera);

		mRenderData.nextCameraProjection = cameraComp->getProjection();
		mRenderData.next.projection = mRenderData.nextCameraProjection.getProjectionsMatrix();
		mRenderData.next.view = transformComp->getViewMatrix();
		mRenderData.next.PV = mRenderData.next.projection * mRenderData.next.view;

		mRenderData.mNextCameraPos = transformComp->getPos(true);
		mRenderData.mNextViewDir = normalize(transformComp->getForward());
		mRenderData.mNextCamFrustum = cameraComp->getFrustum();

		for (const auto renderPass : mRenderPasses) {
			renderPass->render(mRenderData);
		}
	}

	void RenderSystem::debugUpdate(float dt) {
		auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();

		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Render mode")) {
					if (ImGui::RadioButton("default", renderData.mRenderType == SystemsModule::RenderMode::DEFAULT)) {
						renderData.mRenderType = SystemsModule::RenderMode::DEFAULT;
					}
					if (ImGui::RadioButton("wireframe", renderData.mRenderType == SystemsModule::RenderMode::WIREFRAME)) {
						renderData.mRenderType = SystemsModule::RenderMode::WIREFRAME;
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
				float size = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
				if (ImGui::TreeNode("gAlbedoSpec")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData->albedoBuffer.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("gPosition")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData->positionBuffer.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("gNormal")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData->normalBuffer.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("gOutlines")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData->outlinesBuffer.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("gLights")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData->lightsBuffer.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("ssao")) {
					ImGui::Text("mSsaoColorBuffer");
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mSSAOPassData->mSsaoColorBuffer.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });

					ImGui::Text("mSsaoColorBufferBlur");
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mSSAOPassData->mSsaoColorBufferBlur.mId)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
			}
			ImGui::End();
		}
	}

	RenderSystem::~RenderSystem() {
		for (const auto renderPass : mRenderPasses) {
			delete renderPass;
		}
	}
}
