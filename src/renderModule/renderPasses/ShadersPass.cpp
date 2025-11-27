#include "ShadersPass.h"
#include "componentsModule/ModelComponent.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
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
		pipelineDesc.cullMode = CullMode::Back;
		pipelineDesc.frontFace = FrontFace::CounterClockWise;
		pipelineDesc.fillMode = PolygonFillMode::Fill;
		pipelineDesc.depthStencilState = { true, true, CompareOp::LessEqual };
		pipelineDesc.blend = { false };
		pipeline = rhi->createGraphicsPipeline(pipelineDesc);
	}

	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(camera);

	Engine::instance()->rhi()->bindFramebuffer(renderDataHandle.mGeometryPassData->gFramebuffer);

	Engine::instance()->rhi()->cmdBindPipeline(cmd, pipeline);
	EBO.bindIndex(IndexType::Uint32, 0);
	VBO.bindVertex(0);

	Engine::instance()->rhi()->setUniform(pipeline, "cameraPos", renderDataHandle.mCameraPos);
	Engine::instance()->rhi()->setUniform(pipeline, "far", cameraComp->getProjection().getFar());
	Engine::instance()->rhi()->setUniform(pipeline, "near", cameraComp->getProjection().getNear());

	Engine::instance()->rhi()->drawIndexed(6, 0, 1, IndexType::Uint32, 0);

	const auto& drawableEntities = ECSHandler::getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}
	Engine::instance()->rhi()->bindFramebuffer(renderDataHandle.mGeometryPassData->gFramebuffer);

	auto& cameraPos = ECSHandler::registry().pinComponent<const TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos();
	Batcher batcher;
	auto flush = [this, &renderDataHandle, &batcher, &cameraPos](size_t shaderId) {
		const auto shader = SHADER_CONTROLLER->getShader(shaderId);

		shader->use();

		shader->setUniform("P", renderDataHandle.current.projection);
		shader->setUniform("V", renderDataHandle.current.view);
		shader->setUniform("PV", renderDataHandle.current.PV);

		Engine::instance()->rhi()->bindTexture(0, renderDataHandle.mGeometryPassData->positionBuffer, renderDataHandle.mGeometryPassData->sampler);
		Engine::instance()->rhi()->bindTexture(1, renderDataHandle.mGeometryPassData->normalBuffer, renderDataHandle.mGeometryPassData->sampler);
		Engine::instance()->rhi()->bindTexture(2, renderDataHandle.mGeometryPassData->albedoBuffer, renderDataHandle.mGeometryPassData->sampler);

		shader->setUniform("gPosition", 0);
		shader->setUniform("gNormal", 1);
		shader->setUniform("gAlbedoSpec", 2);
		batcher.sort(cameraPos);
		batcher.flushAll({}); //todo
		batcher.clear();
	};

	size_t curShaderId = drawableEntities.front().first;
	for (auto& [shaderId, entityId] : drawableEntities) {
		if (shaderId != curShaderId) {
			flush(shaderId);

			curShaderId = shaderId;
		}

		/*if (auto modelComp = ECSHandler::registry().getComponent<ModelComponent>(entityId)) {
			auto& transform = ECSHandler::registry().getComponent<TransformComponent>(entityId)->getTransform();
			auto& model = modelComp->getModel();
			for (auto& mesh : model.meshes) {
				if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform)) {
					batcher.addToDrawList(mesh.vertices->mVao, mesh.vertices->vertices.size(), mesh.vertices->indices.size(), *mesh.mMaterial, transform);

				}
			}
		}*/
	}

	flush(curShaderId);
	Engine::instance()->rhi()->bindFramebuffer({});
	
}
