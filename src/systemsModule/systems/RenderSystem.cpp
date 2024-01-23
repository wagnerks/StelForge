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

		std::sort(mRenderPasses.begin(), mRenderPasses.end(), [](RenderModule::RenderPass* a, RenderModule::RenderPass* b) {
			return a->getPriority() < b->getPriority();
		});
	}

	RenderSystem::RenderSystem(RenderModule::Renderer* renderer) : mRenderer(renderer) {
		mRenderPasses.reserve(RENDER_PASSES_PRIORITY.size());

		addRenderPass<RenderModule::RenderPasses::CascadedShadowPass>();
		addRenderPass<RenderModule::RenderPasses::PointLightPass>();
		addRenderPass<RenderModule::RenderPasses::GeometryPass>();
		addRenderPass<RenderModule::RenderPasses::ShadersPass>();
		addRenderPass<RenderModule::RenderPasses::LightingPass>();
		addRenderPass<RenderModule::RenderPasses::SSAOPass>();
		addRenderPass<RenderModule::RenderPasses::DebugPass>();

		glGenBuffers(1, &cameraMatricesUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, cameraMatricesUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(RenderMatrices), nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, cameraMatricesUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void RenderSystem::update(float_t dt) {
		if (!mRenderer) {
			return;
		}
		FUNCTION_BENCHMARK;

		auto& compManager = ECSHandler::registry();

		auto& playerCamera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();

		mRenderData.current = mRenderData.next;
		mRenderData.cameraProjection = mRenderData.nextCameraProjection;

		mRenderData.mCameraPos = mRenderData.mNextCameraPos;
		mRenderData.mCamFrustum = mRenderData.mNextCamFrustum;

		glBindBuffer(GL_UNIFORM_BUFFER, cameraMatricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RenderMatrices), &mRenderData.current);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		mRenderData.nextCameraProjection = compManager.getComponent<CameraComponent>(playerCamera)->getProjection();
		mRenderData.next.projection = mRenderData.nextCameraProjection.getProjectionsMatrix();
		mRenderData.next.view = compManager.getComponent<TransformComponent>(playerCamera)->getViewMatrix();
		mRenderData.next.PV = mRenderData.next.projection * mRenderData.next.view;

		mRenderData.mNextCameraPos = compManager.getComponent<TransformComponent>(playerCamera)->getPos(true);
		mRenderData.mNextCamFrustum = ECSHandler::registry().getComponent<CameraComponent>(playerCamera)->getFrustum();

		for (const auto renderPass : mRenderPasses) {
			renderPass->render(mRenderer, mRenderData, *mRenderer->getBatcher());
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
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData.gAlbedoSpec)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("gPosition")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData.gPosition)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("gNormal")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData.gNormal)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("gOutlines")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData.gOutlines)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("gLights")) {
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mGeometryPassData.gLights)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("ssao")) {
					ImGui::Text("mSsaoColorBuffer");
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mSSAOPassData.mSsaoColorBuffer)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });

					ImGui::Text("mSsaoColorBufferBlur");
					ImGui::Image(reinterpret_cast<void*>(static_cast<size_t>(renderData.mSSAOPassData.mSsaoColorBufferBlur)), { size,size }, { 0.f, 1.f }, { 1.f,0.f });
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
