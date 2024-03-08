#include "DebugPass.h"

#include "imgui.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Draw.h"
#include "glWrapper/VertexArray.h"
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
		linesVAO.bind();
		linesVBO.bind();

		linesVAO.addAttribute(0, 3, GLW::AttributeFType::FLOAT, false, 3 * sizeof(float));

		linesVAO.bindDefault();
		linesVBO.unbind();


		trianglesVAO.generate();
		trianglesVAO.bind();
		trianglesVBO.bind();

		trianglesVAO.addAttribute(0, 3, GLW::AttributeFType::FLOAT, false, &SFE::Vertex3D::position);
		trianglesVAO.addAttribute(1, 3, GLW::AttributeFType::FLOAT, true,  &SFE::Vertex3D::normal);
		trianglesVAO.addAttribute(2, 4, GLW::AttributeFType::FLOAT, true,  &SFE::Vertex3D::color);

		trianglesVAO.bindDefault();
		trianglesVBO.unbind();
	}

	DebugPass::~DebugPass() {}
	

		
	void DebugPass::render(SystemsModule::RenderData& renderDataHandle) {
		FUNCTION_BENCHMARK;

		if (!Utils::renderTriangles.empty()) {
			auto triangleShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/simpleColoredTriangle.vs", "shaders/simpleColoredTriangle.fs");
			triangleShader->use();
			triangleShader->setUniform("PVM", renderDataHandle.current.PV);
			triangleShader->setUniform("viewPos", renderDataHandle.mCameraPos);
			trianglesVBO.bind();
			for (auto& [data, triangles] : Utils::renderTriangles) {
				trianglesVBO.allocateData(triangles, GLW::STATIC_DRAW);


				GLW::CapabilitiesStack<GLW::BLEND>::push(data.blend);
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::push(data.depthTest);
				GLW::CapabilitiesStack<GLW::CULL_FACE>::push(data.cull);
				GLW::BlendFuncStack::push({ GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });
				GLW::drawVertices(GLW::TRIANGLES, trianglesVAO.getID(), static_cast<int>(triangles.size()) * 3);
				GLW::BlendFuncStack::pop();
				GLW::CapabilitiesStack<GLW::BLEND>::pop();
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::pop();
				GLW::CapabilitiesStack<GLW::CULL_FACE>::pop();
			}
			trianglesVBO.unbind();

			Utils::renderTriangles.clear();
		}
		

		if (!Utils::renderVertices.empty()) {
			auto coloredLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
			coloredLines->use();
			coloredLines->setUniform("PVM", renderDataHandle.current.PV);

			linesVBO.bind();
			for (auto& [data, vertices] : Utils::renderVertices) {
				coloredLines->setUniform("color", data.color);


				linesVBO.allocateData(vertices, GLW::STATIC_DRAW);


				GLW::CapabilitiesStack<GLW::BLEND>::push(true);
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::push(false);
				GLW::BlendFuncStack::push({ GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });

				GLW::LineWidth::push(data.thickness);
				GLW::drawVertices(data.renderType, linesVAO.getID(), static_cast<int>(vertices.size()), 0, 0);
				GLW::LineWidth::pop();

				GLW::BlendFuncStack::pop();
				GLW::CapabilitiesStack<GLW::BLEND>::pop();
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::pop();
			}
			linesVBO.unbind();
			Utils::renderVertices.clear();
		}
		


		//static SceneGridFloor grid;
		//grid.draw();

		auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
		CascadeShadowComponent::debugDraw(ECSHandler::registry().getComponent<CascadeShadowComponent>(renderData.mCascadedShadowsPassData->shadows)->getCacheLightSpaceMatrices(), renderData.next.projection, renderData.next.view);

		if (!renderData.mCascadedShadowsPassData->shadowCascadeLevels.empty() && ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->isShadowsDebugData()) {
			auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
			sh->use();
			sh->setUniform("depthMap", 31);

			auto a = 250.f / Render::Renderer::screenDrawData.width;
			auto b = 250.f / Render::Renderer::screenDrawData.height;

			for (auto i = 0; i < renderData.mCascadedShadowsPassData->shadowCascadeLevels.size() - 1; i++) {
				sh->setUniform("near_plane", renderData.mCascadedShadowsPassData->shadowCascadeLevels[i]);
				sh->setUniform("far_plane", renderData.mCascadedShadowsPassData->shadowCascadeLevels[i + 1]);
				sh->setUniform("layer", i);

				Render::Utils::renderQuad(1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
			}
		}
	}
}

