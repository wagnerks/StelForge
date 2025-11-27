#include "DebugPass.h"

#include "imgui.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/GizmoComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Draw.h"
#include "glWrapper/VertexArray.h"
#include "renderModule/SceneGridFloor.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

namespace SFE::Render::RenderPasses {
	DebugPass::DebugPass() {
		auto rhi = Engine::instance()->rhi();

		BufferDesc lineBufDesc;
		lineBufDesc.size = 1000 * sizeof(float) * 3;
		lineBufDesc.usage = BufferUsage::Vertex;
		lineBufDesc.cpuAccess = CpuAccess::Write;
		linesVBO = rhi->createBuffer(lineBufDesc);

		VertexLayoutDesc lineLayout;
		lineLayout.bindings.push_back({ 0, sizeof(float) * 3, VertexInputRate::PerVertex });
		lineLayout.attributes.push_back({
			0,                            // location 0
			0,
			VertexFormat::Float3,         // vec3 position
			0,
			false                         // normalized
			});
		

		BufferDesc triBufDesc;
		triBufDesc.size = 500 * sizeof(SFE::Vertex3D);
		triBufDesc.usage = BufferUsage::Vertex;
		triBufDesc.cpuAccess = CpuAccess::Write;
		trianglesVBO = rhi->createBuffer(triBufDesc);

		VertexLayoutDesc triLayout;
		triLayout.bindings.push_back({ 0, sizeof(SFE::Vertex3D), VertexInputRate::PerVertex });

		triLayout.attributes.push_back({
			0,                            // location 0
			0,
			VertexFormat::Float3,         // vec3 position
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, position)),
			false                         // normalized
			});

		triLayout.attributes.push_back({
			1,                            // location 1
			0,
			VertexFormat::Float3,         // vec3 normal
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)),
			true                          // normalized = true
			});

		triLayout.attributes.push_back({
			2,                            // location 2
			0,
			VertexFormat::Float4,         // vec4 color
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, color)),
			true                          // normalized = true
			});

		ShaderModuleDesc triangleVS;
		triangleVS.stage = ShaderStage::Vertex;
		triangleVS.name = "Debug Triangle VS";
		triangleVS.source = ShaderModule::ShaderBase::loadShaderCode("shaders/simpleColoredTriangle.vs");

		ShaderModuleDesc triangleFS;
		triangleFS.stage = ShaderStage::Fragment;
		triangleFS.name = "Debug Triangle FS";
		triangleFS.source = ShaderModule::ShaderBase::loadShaderCode("shaders/simpleColoredTriangle.fs");

		GraphicsPipelineDesc debugPipelineDesc{};
		debugPipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule(triangleVS);
		debugPipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule(triangleFS);
		debugPipelineDesc.topology = PrimitiveTopology::Triangles;
		debugPipelineDesc.cullMode = CullMode::Back;
		debugPipelineDesc.frontFace = FrontFace::CounterClockWise;
		debugPipelineDesc.fillMode = PolygonFillMode::Fill;
		debugPipelineDesc.depthStencilState.depthTest = true;
		debugPipelineDesc.depthStencilState.depthWrite = true;
		debugPipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
		debugPipelineDesc.blend.enabled = false;
		debugPipelineDesc.layout = triLayout;
		trianglesPipeline = rhi->createGraphicsPipeline(debugPipelineDesc);

		GraphicsPipelineDesc linePipelineDesc;
		linePipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule(ShaderModuleDesc{
			ShaderStage::Vertex, "Debug Lines VS", ShaderModule::ShaderBase::loadShaderCode("shaders/xyzLines.vs")});

		linePipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule(ShaderModuleDesc{
			ShaderStage::Fragment, "Debug Lines FS", ShaderModule::ShaderBase::loadShaderCode("shaders/colored_lines.fs")});

		linePipelineDesc.topology = PrimitiveTopology::Lines;
		linePipelineDesc.cullMode = CullMode::None;
		linePipelineDesc.frontFace = FrontFace::CounterClockWise;
		linePipelineDesc.fillMode = PolygonFillMode::Line;
		linePipelineDesc.depthStencilState.depthTest = false;
		linePipelineDesc.depthStencilState.depthWrite = false;
		linePipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
		linePipelineDesc.blend.enabled = true;
		linePipelineDesc.blend.srcColor = BlendFactor::SrcAlpha;
		linesPipeline = rhi->createGraphicsPipeline(linePipelineDesc);
	}

	DebugPass::~DebugPass() {}

	void DebugPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		FUNCTION_BENCHMARK;

		for (auto [entId, gizmoComp] : ECSHandler::registry().view<ComponentsModule::GizmoComponent>()) {
			gizmoComp->gizmo.setEntity(entId);
			gizmoComp->gizmo.update();
		}

		if (!Utils::renderTriangles.empty()) {
			Engine::instance()->rhi()->cmdBindPipeline(cmd, trianglesPipeline);
			trianglesVBO.bindVertex(0);
			
			Engine::instance()->rhi()->setUniform(trianglesPipeline, "PVM", renderDataHandle.current.PV);
			Engine::instance()->rhi()->setUniform(trianglesPipeline, "viewPos", renderDataHandle.mCameraPos);
			

			for (auto& [data, triangles] : Utils::renderTriangles) {
				Engine::instance()->rhi()->updateBuffer(trianglesVBO,0, triangles.size() * sizeof(SFE::Vertex3D) * 3, triangles.data());

				GLW::CapabilitiesStack<GLW::BLEND>::push(data.blend);
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::push(data.depthTest);
				GLW::CapabilitiesStack<GLW::CULL_FACE>::push(data.cull);
				GLW::BlendFuncStack::push({ GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });
				GLW::drawVerticesW(GLW::TRIANGLES, static_cast<int>(triangles.size()) * 3);
				GLW::BlendFuncStack::pop();
				GLW::CapabilitiesStack<GLW::BLEND>::pop();
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::pop();
				GLW::CapabilitiesStack<GLW::CULL_FACE>::pop();
			}

			Utils::renderTriangles.clear();
		}
		

		if (!Utils::renderVertices.empty()) {
			Engine::instance()->rhi()->cmdBindPipeline(cmd, linesPipeline);
			linesVBO.bindVertex(0);

			Engine::instance()->rhi()->setUniform(linesPipeline, "PVM", renderDataHandle.current.PV);
			for (auto& [data, vertices] : Utils::renderVertices) {
				Engine::instance()->rhi()->setUniform(linesPipeline, "color", data.color);
				Engine::instance()->rhi()->updateBuffer(trianglesVBO, 0, vertices.size() * sizeof(SFE::Vertex3D) , vertices.data());

				GLW::CapabilitiesStack<GLW::BLEND>::push(true);
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::push(false);
				GLW::BlendFuncStack::push({ GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });

				GLW::LineWidth::push(data.thickness);
				GLW::drawVerticesW(data.renderType, static_cast<int>(vertices.size()), 0, 0);
				GLW::LineWidth::pop();

				GLW::BlendFuncStack::pop();
				GLW::CapabilitiesStack<GLW::BLEND>::pop();
				GLW::CapabilitiesStack<GLW::DEPTH_TEST>::pop();
			}

			Utils::renderVertices.clear();
		}
		


		//static SceneGridFloor grid;
		//grid.draw();

		auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
		CascadeShadowComponent::debugDraw(ECSHandler::registry().pinComponent<const CascadeShadowComponent>(renderData.mCascadedShadowsPassData->shadows)->getCacheLightSpaceMatrices(), renderData.next.projection, renderData.next.view);

		if (!renderData.mCascadedShadowsPassData->shadowCascadeLevels.empty() && ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->isShadowsDebugData()) {
			auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
			sh->use();
			sh->setUniform("depthMap", 31);
			
			auto a = 250.f / Engine::instance()->getWindow()->getScreenData().width;
			auto b = 250.f / Engine::instance()->getWindow()->getScreenData().height;

			for (auto i = 0; i < renderData.mCascadedShadowsPassData->shadowCascadeLevels.size() - 1; i++) {
				sh->setUniform("near_plane", renderData.mCascadedShadowsPassData->shadowCascadeLevels[i]);
				sh->setUniform("far_plane", renderData.mCascadedShadowsPassData->shadowCascadeLevels[i + 1]);
				sh->setUniform("layer", i);

				Render::Utils::renderQuad(1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
			}
		}
	}
}

