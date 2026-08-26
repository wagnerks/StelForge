#include "DebugPass.h"

#include "imgui.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/GizmoComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
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
		linePipelineDesc.blend.dstColor = BlendFactor::OneMinusSrcAlpha;
		linesPipeline = rhi->createGraphicsPipeline(linePipelineDesc);

		RenderPassDesc rpd;
		rpd.colorAttachmentCount = 1;
		rpd.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
		rpd.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		debugRenderPass = rhi->createRenderPass(rpd);
	}

	DebugPass::~DebugPass() {}

	void DebugPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		FUNCTION_BENCHMARK;

		for (auto [entId, gizmoComp] : ECSHandler::registry().view<ComponentsModule::GizmoComponent>()) {
			gizmoComp->gizmo.setEntity(entId);
			gizmoComp->gizmo.update();
		}
		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.renderPass = debugRenderPass;
		rpBeginInfo.framebuffer = Engine::instance()->rhi()->getBackbufferFramebuffer();
		rpBeginInfo.width = Engine::instance()->getWindow()->getScreenData().renderW;
		rpBeginInfo.height = Engine::instance()->getWindow()->getScreenData().renderH;

		Engine::instance()->rhi()->cmdBeginRenderPass(cmd, rpBeginInfo);
		if (!Utils::renderTriangles.empty()) {
			Engine::instance()->rhi()->cmdBindPipeline(cmd, trianglesPipeline);
			Engine::instance()->rhi()->cmdBindVertexBuffer(cmd, 0, trianglesVBO);
			drawDataTriangles.pv = renderDataHandle.current.PV;
			drawDataTriangles.viewPos = Math::Vec4(renderDataHandle.mCameraPos, 1.f);
			Engine::instance()->rhi()->cmdPushConstants(cmd, trianglesPipeline, drawDataTriangles);

			for (auto& [data, triangles] : Utils::renderTriangles) {
				Engine::instance()->rhi()->updateBuffer(trianglesVBO,0, triangles.size() * sizeof(SFE::Vertex3D) * 3, triangles.data());
				Engine::instance()->rhi()->cmdBindPipeline(cmd, getTrianglePipeline(data.blend, data.depthTest, data.cull, { BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha }));
				Engine::instance()->rhi()->cmdDraw(cmd, static_cast<int>(triangles.size()) * 3);
			}

			Utils::renderTriangles.clear();
		}

		if (!Utils::renderVertices.empty()) {
			Engine::instance()->rhi()->cmdBindPipeline(cmd, linesPipeline);
			Engine::instance()->rhi()->cmdBindVertexBuffer(cmd, 0, linesVBO);
			drawDataLines.pv = renderDataHandle.current.PV;
			Engine::instance()->rhi()->cmdPushConstants(cmd, linesPipeline, drawDataLines);
			for (auto& [data, vertices] : Utils::renderVertices) {
				drawDataLines.color = data.color;
				Engine::instance()->rhi()->cmdPushConstants(cmd, linesPipeline, drawDataLines);

				Engine::instance()->rhi()->updateBuffer(linesVBO, 0, vertices.size() * sizeof(SFE::Vertex3D) , vertices.data());
				Engine::instance()->rhi()->cmdDraw(cmd, static_cast<int>(vertices.size()));
			}

			Utils::renderVertices.clear();
		}
		
		Engine::instance()->rhi()->cmdEndRenderPass(cmd);

		auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
		CascadeShadowComponent::debugDraw(ECSHandler::registry().pinComponent<const CascadeShadowComponent>(renderData.mCascadedShadowsPassData->shadows)->getCacheLightSpaceMatrices(), renderData.next.projection, renderData.next.view);

		//if (!renderData.mCascadedShadowsPassData->shadowCascadeLevels.empty() && ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->isShadowsDebugData()) {
		//	//auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
		//	sh->use();
		//	sh->setUniform("depthMap", 31);
		//	
		//	auto a = 250.f / Engine::instance()->getWindow()->getScreenData().width;
		//	auto b = 250.f / Engine::instance()->getWindow()->getScreenData().height;

		//	for (auto i = 0; i < renderData.mCascadedShadowsPassData->shadowCascadeLevels.size() - 1; i++) {
		//		sh->setUniform("near_plane", renderData.mCascadedShadowsPassData->shadowCascadeLevels[i]);
		//		sh->setUniform("far_plane", renderData.mCascadedShadowsPassData->shadowCascadeLevels[i + 1]);
		//		sh->setUniform("layer", i);

		//		Render::Utils::renderQuad(cmd, 1.f - a, 1.f - (static_cast<float>(i) + 1.f) * b, 1.f, 1.f - static_cast<float>(i) * b);
		//	}
		//}
	}

	PipelineHandle DebugPass::getTrianglePipeline(bool blend, bool depthTest, bool cull, std::pair<BlendFactor, BlendFactor> blendFactors)
	{
		PipelineDesc desc;
		desc.blend = blend;
		desc.depthTest = depthTest;
		desc.cull = cull;
		desc.blendFactors = blendFactors;

		if (!trianglesPipelinesCache.contains(desc)) {
			GraphicsPipelineDesc debugPipelineDesc{};
			debugPipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule(ShaderModuleDesc{
				ShaderStage::Vertex, "Debug Triangle VS", ShaderModule::ShaderBase::loadShaderCode("shaders/simpleColoredTriangle.vs") });
			debugPipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule(ShaderModuleDesc{
				ShaderStage::Fragment, "Debug Triangle FS", ShaderModule::ShaderBase::loadShaderCode("shaders/simpleColoredTriangle.fs") });

			debugPipelineDesc.topology = PrimitiveTopology::Triangles;
			debugPipelineDesc.cullMode = cull ? CullMode::Back : CullMode::None;
			debugPipelineDesc.frontFace = FrontFace::CounterClockWise;
			debugPipelineDesc.fillMode = PolygonFillMode::Fill;
			debugPipelineDesc.depthStencilState.depthTest = depthTest;
			debugPipelineDesc.depthStencilState.depthWrite = depthTest;
			debugPipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
			debugPipelineDesc.blend.enabled = blend;
			if (blend) {
				debugPipelineDesc.blend.srcColor = blendFactors.first;
				debugPipelineDesc.blend.dstColor = blendFactors.second;
			}
			debugPipelineDesc.layout = VertexLayoutDesc{};
			debugPipelineDesc.layout.bindings.push_back({ 0, sizeof(SFE::Vertex3D), VertexInputRate::PerVertex });
			debugPipelineDesc.layout.attributes.push_back({
				0,                            // location 0
				0,
				VertexFormat::Float3,         // vec3 position
				static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, position)),
				false                         // normalized
				});
			debugPipelineDesc.layout.attributes.push_back({
				1,                            // location 1
				0,
				VertexFormat::Float3,         // vec3 normal
				static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)),
				true                          // normalized = true
				});
			debugPipelineDesc.layout.attributes.push_back({
				2,                            // location 2
				0,
				VertexFormat::Float4,         // vec4 color
				static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, color)),
				true                          // normalized = true
				});

			trianglesPipelinesCache[desc] = Engine::instance()->rhi()->createGraphicsPipeline(debugPipelineDesc);
		}

		return trianglesPipelinesCache[desc];
	}
}

