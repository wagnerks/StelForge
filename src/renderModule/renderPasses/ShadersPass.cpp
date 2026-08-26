#include "ShadersPass.h"
#include "componentsModule/ModelComponent.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/ShaderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"


SFE::Render::RenderPasses::ShadersPass::ShadersPass() {}

void SFE::Render::RenderPasses::ShadersPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	static PipelineHandle pipeline;
	static BufferHandle VBO;
	static BufferHandle EBO;

	if (!pipeline) {
		auto* rhi = Engine::instance()->rhi();

		std::vector<SFE::Vertex3D> vertices = {
			{{ 1.f, 0.f, -1.f}, {1.f,1.f}, {0.f,1.f,0.f}}, //far right
			{{ 1.f, 0.f,  1.f}, {1.f,0.f}, {0.f,1.f,0.f}}, //near right
			{{-1.f, 0.f,  1.f}, {0.f,0.f}, {0.f,1.f,0.f}}, //near left
			{{-1.f, 0.f, -1.f}, {0.f,1.f}, {0.f,1.f,0.f}}, //far left
		};

		std::vector<unsigned> indices = {
			0,2,1,
			3,2,0
		};

		BufferDesc vboDesc;
		vboDesc.size = vertices.size() * sizeof(SFE::Vertex3D);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::None;
		VBO = rhi->createBuffer(vboDesc, vertices.data());

		BufferDesc eboDesc;
		eboDesc.size = indices.size() * sizeof(unsigned);
		eboDesc.usage = BufferUsage::Index;
		eboDesc.cpuAccess = CpuAccess::None;
		EBO = rhi->createBuffer(eboDesc, indices.data());

		VertexLayoutDesc vaoDesc;

		vaoDesc.bindings.push_back({ 0, sizeof(SFE::Vertex3D), VertexInputRate::PerVertex });

		vaoDesc.attributes.push_back({
			0,                            // location 0
			0,
			VertexFormat::Float3,         // vec3 position
			static_cast<std::uint32_t>(offsetof(Vertex3D, position)),
			false                         // normalized
			});
		vaoDesc.attributes.push_back({
			1,                            // location 1
			0,
			VertexFormat::Float3,         // vec3 normal
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)),
			true                          // normalized = true
			});
		vaoDesc.attributes.push_back({
			2,                            // location 2
			0,
			VertexFormat::Float2,         // vec2 texCoords
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, texCoords)),
			true                          // normalized = true
			});

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.layout = vaoDesc;
		pipelineDesc.shaders.vertex = rhi->createShaderModule({ ShaderStage::Vertex, "Shaders Pass Vertex Shader", ShaderModule::ShaderBase::loadShaderCode("shaders/testSh.vs") });
		pipelineDesc.shaders.fragment = rhi->createShaderModule({ ShaderStage::Fragment, "Shaders Pass Fragment Shader", ShaderModule::ShaderBase::loadShaderCode("shaders/testSh.fs") });
		pipelineDesc.topology = PrimitiveTopology::Triangles;
		pipelineDesc.cullMode = CullMode::None;
		pipelineDesc.frontFace = FrontFace::CounterClockWise;
		pipelineDesc.fillMode = PolygonFillMode::Fill;
		pipelineDesc.depthStencilState = { true, true, CompareOp::LessEqual };
		pipelineDesc.blend = { false };
		pipeline = rhi->createGraphicsPipeline(pipelineDesc);

		RenderPassDesc renderPassDesc;
		renderPassDesc.colorAttachmentCount = 4;
		renderPassDesc.colorAttachments[0] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store };
		renderPassDesc.colorAttachments[1] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store };
		renderPassDesc.colorAttachments[2] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store };
		renderPassDesc.colorAttachments[3] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store };
		renderPassDesc.hasDepthStencil = true;
		renderPassDesc.depthAttachment = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, 1.f, 0, false };
		renderPass = rhi->createRenderPass(renderPassDesc);
		pipelineDesc.renderPass = renderPass;
	}

	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(camera);
	auto w = Engine::instance()->getWindow()->getScreenData().renderW;
	auto h = Engine::instance()->getWindow()->getScreenData().renderH;
	
	Engine::instance()->rhi()->cmdBeginRenderPass(cmd, {
		.renderPass = renderPass,
		.framebuffer = renderDataHandle.mGeometryPassData->gFramebuffer,
		.width = w,
		.height = h,
	});
	Engine::instance()->rhi()->cmdBindPipeline(cmd, pipeline);
	Engine::instance()->rhi()->cmdBindIndexBuffer(cmd, EBO, IndexType::Uint32);
	Engine::instance()->rhi()->cmdBindVertexBuffer(cmd, 0, VBO);
	drawData.viewPos.xyz = renderDataHandle.mCameraPos;
	drawData.far = cameraComp->getProjection().getFar();
	drawData.near = cameraComp->getProjection().getNear();
	Engine::instance()->rhi()->cmdPushConstants(cmd, pipeline, drawData);

	Engine::instance()->rhi()->cmdDrawIndexed(cmd, 6);
	Engine::instance()->rhi()->cmdEndRenderPass(cmd);

	const auto& drawableEntities = ECSHandler::getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}
	//Engine::instance()->rhi()->bindFramebuffer(renderDataHandle.mGeometryPassData->gFramebuffer);
}
