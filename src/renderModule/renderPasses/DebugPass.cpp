#include "DebugPass.h"

#include "imgui.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "glad/glad.h"
#include "renderModule/CapabilitiesStack.h"
#include "renderModule/Renderer.h"
#include "renderModule/SceneGridFloor.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

namespace SFE::Render::RenderPasses {
	DebugPass::DebugPass() {
		linesVAO.generate();
	}

	DebugPass::~DebugPass() {}
	

		
	void DebugPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
		FUNCTION_BENCHMARK;

		
		auto triangleShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/simpleColoredTriangle.vs", "shaders/simpleColoredTriangle.fs");
		triangleShader->use();
		triangleShader->setMat4("PVM", renderDataHandle.current.PV);
		triangleShader->setVec3("viewPos", renderDataHandle.mCameraPos);

		
		linesVAO.bind();
		cubeVBO.bind();

		linesVAO.addAttribute(0, 3, FLOAT, false, &SFE::Render::Utils::LightVertex::position);
		linesVAO.addAttribute(1, 3, FLOAT, true, &SFE::Render::Utils::LightVertex::normal);

		for (auto& [data, triangles] : Utils::renderTriangles) {
			triangleShader->setVec4("color", data.color);

			cubeVBO.allocateData(triangles.size(), STATIC_DRAW, triangles.data());


			CapabilitiesStack::push(BLEND, data.blend);
			CapabilitiesStack::push(DEPTH_TEST, data.depthTest);
			CapabilitiesStack::push(CULL_FACE, data.cull);

			Render::Renderer::drawArrays(TRIANGLES, static_cast<int>(triangles.size()) * 3);

			CapabilitiesStack::pop();
			CapabilitiesStack::pop();
			CapabilitiesStack::pop();
		}

		cubeVBO.allocateData<Utils::Triangle>(0, STATIC_DRAW, nullptr);

		Utils::renderTriangles.clear();

		auto coloredLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
		coloredLines->use();
		coloredLines->setMat4("PVM", renderDataHandle.current.PV);

		linesVAO.addAttribute(0, 3, FLOAT, false, 3 * sizeof(float));

		for (auto& [data, vertices] : Utils::renderVertices) {
			coloredLines->setVec4("color", data.color);

			cubeVBO.allocateData(vertices.size(), STATIC_DRAW, vertices.data());

			CapabilitiesStack::push(BLEND, true);
			CapabilitiesStack::push(DEPTH_TEST, false);

			glLineWidth(data.thickness);
			
			Render::Renderer::drawArrays(data.renderType, static_cast<int>(vertices.size()));

			glLineWidth(1.f);
			

			CapabilitiesStack::pop();
			CapabilitiesStack::pop();
		}

		cubeVBO.allocateData<Math::Vec3>(0, STATIC_DRAW, nullptr);

		VertexArray::bindDefault();
		Buffer::bindDefaultBuffer(cubeVBO.getType());

		Utils::renderVertices.clear();


		static SceneGridFloor grid;
		//grid.draw();

		auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
		CascadeShadowComponent::debugDraw(ECSHandler::registry().getComponent<CascadeShadowComponent>(renderData.mCascadedShadowsPassData.shadows)->getCacheLightSpaceMatrices(), renderData.next.projection, renderData.next.view);

		if (!renderData.mCascadedShadowsPassData.shadowCascadeLevels.empty() && ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->isShadowsDebugData()) {
			auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
			sh->use();
			sh->setInt("depthMap", 31);

			auto a = 250.f / Render::Renderer::SCR_WIDTH;
			auto b = 250.f / Render::Renderer::SCR_HEIGHT;

			for (auto i = 0; i < renderData.mCascadedShadowsPassData.shadowCascadeLevels.size() - 1; i++) {
				sh->setFloat("near_plane", renderData.mCascadedShadowsPassData.shadowCascadeLevels[i]);
				sh->setFloat("far_plane", renderData.mCascadedShadowsPassData.shadowCascadeLevels[i + 1]);
				sh->setInt("layer", i);

				Render::Utils::renderQuad(1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
			}
		}
	}
}

