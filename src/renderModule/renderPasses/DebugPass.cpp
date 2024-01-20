#include "DebugPass.h"

#include "imgui.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "glad/glad.h"
#include "renderModule/Renderer.h"
#include "renderModule/SceneGridFloor.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

namespace Engine::RenderModule::RenderPasses {
	DebugPass::DebugPass() {

		glGenVertexArrays(1, &linesVAO);
		glGenBuffers(1, &cubeVBO);
	}

	DebugPass::~DebugPass() {
		glDeleteVertexArrays(1, &linesVAO);
		glDeleteBuffers(1, &cubeVBO);
	}

	void DebugPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
		FUNCTION_BENCHMARK;

		auto coloredLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
		coloredLines->use();
		coloredLines->setMat4("PVM", renderDataHandle.current.PV);

		for (auto& [color, vertices] : Utils::renderVertices) {
			coloredLines->setVec4("color", color);


			glBindVertexArray(linesVAO);
			glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices.front()) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

			RenderModule::Renderer::drawArrays(GL_LINES, vertices.size());

			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			glBindVertexArray(0);
		}
		
		Utils::renderVertices.clear();

		static SceneGridFloor grid;
		grid.draw();

		auto& renderData = ECSHandler::getSystem<Engine::SystemsModule::RenderSystem>()->getRenderData();
		CascadeShadowComponent::debugDraw(ECSHandler::registry().getComponent<CascadeShadowComponent>(renderData.mCascadedShadowsPassData.shadows)->getCacheLightSpaceMatrices(), renderData.next.projection, renderData.next.view);

		if (!renderData.mCascadedShadowsPassData.shadowCascadeLevels.empty() && ECSHandler::getSystem<Engine::SystemsModule::RenderSystem>()->isShadowsDebugData()) {
			auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
			sh->use();
			sh->setInt("depthMap", 31);

			auto a = 250.f / RenderModule::Renderer::SCR_WIDTH;
			auto b = 250.f / RenderModule::Renderer::SCR_HEIGHT;

			for (auto i = 0; i < renderData.mCascadedShadowsPassData.shadowCascadeLevels.size() - 1; i++) {
				sh->setFloat("near_plane", renderData.mCascadedShadowsPassData.shadowCascadeLevels[i]);
				sh->setFloat("far_plane", renderData.mCascadedShadowsPassData.shadowCascadeLevels[i + 1]);
				sh->setInt("layer", i);

				RenderModule::Utils::renderQuad(1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
			}
		}
	}
}

