#include "DebugPass.h"

#include "imgui.h"
#include "assetsModule/modelModule/ModelLoader.h"
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

namespace SFE::RenderModule::RenderPasses {
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

		for (auto& [data, vertices] : Utils::renderVertices) {
			coloredLines->setVec4("color", data.color);


			glBindVertexArray(linesVAO);
			glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices.front()) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

			glLineWidth(data.thickness);

			auto prevBlendState = glIsEnabled(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			RenderModule::Renderer::drawArrays(data.renderType, vertices.size());
			if (!prevBlendState) {
				glDisable(GL_BLEND);
			}
			glEnable(GL_DEPTH_TEST);
			glLineWidth(1.f);

			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			glBindVertexArray(0);
		}
		
		Utils::renderVertices.clear();

		for (auto& [data, triangles] : Utils::renderTriangles) {
			coloredLines->setVec4("color", data.color);


			glBindVertexArray(linesVAO);
			glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(triangles.front()) * triangles.size(), triangles.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			
			auto prevBlendState = glIsEnabled(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			RenderModule::Renderer::drawArrays(GL_TRIANGLES, triangles.size() * 3);
			if (!prevBlendState) {
				glDisable(GL_BLEND);
			}
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glLineWidth(1.f);

			glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
			glBindVertexArray(0);
		}

		Utils::renderTriangles.clear();

		static SceneGridFloor grid;
		grid.draw();

		auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
		CascadeShadowComponent::debugDraw(ECSHandler::registry().getComponent<CascadeShadowComponent>(renderData.mCascadedShadowsPassData.shadows)->getCacheLightSpaceMatrices(), renderData.next.projection, renderData.next.view);

		if (!renderData.mCascadedShadowsPassData.shadowCascadeLevels.empty() && ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->isShadowsDebugData()) {
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

