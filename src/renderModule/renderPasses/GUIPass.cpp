#include "GUIPass.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/CapabilitiesStack.h"
#include "mathModule/Utils.h"

namespace SFE::Render::RenderPasses {
	GUIPass::~GUIPass() {}

	void GUIPass::init() {
		const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/2dshader.vs", "shaders/2dshader.fs");

		shader->use();
		shader->setUniform("projection", Math::orthoRH_NO(0.0f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().width), 0.f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().height), -1.f, 1.f));

		BufferDesc vboDesc;
		vboDesc.size = sizeof(float) * 2 * 3 * 1000; // max 1000 quads
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		VBO = Engine::instance()->rhi()->createBuffer(vboDesc);

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "GUI Vertex Shader", ShaderModule::ShaderBase::loadShaderCode("shaders/2dshader.vs") });
		pipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Fragment, "GUI Fragment Shader", ShaderModule::ShaderBase::loadShaderCode("shaders/2dshader.fs") });
		pipelineDesc.topology = PrimitiveTopology::Triangles;
		pipelineDesc.cullMode = CullMode::None;
		pipelineDesc.blend = { true, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };
		pipelineDesc.depthStencilState = { false};

		pipelineDesc.layout.bindings.push_back(VertexBindingDesc{ 0, sizeof(Math::Vec2), VertexInputRate::PerVertex });
		pipelineDesc.layout.attributes.push_back(VertexAttributeDesc{
			.location = 0,
			.binding = 0,
			.format = VertexFormat::Float2,
			.offset = 0,
			.normalized = false
			});

		pipeline = Engine::instance()->rhi()->createGraphicsPipeline(pipelineDesc);
	}

	void GUIPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		if (registry.getAllEntities().empty()) {
			return;
		}
		
		Engine::instance()->rhi()->cmdBindPipeline(cmd, pipeline);
		VBO.bindVertex(0);

		const float semiW = Engine::instance()->getWindow()->getScreenData().width * 0.5f;
		const float semiH = Engine::instance()->getWindow()->getScreenData().height * 0.5f;
		int i = 0;
		for (auto [entId, position, color, textComp] : registry.view<PosComponent, ColorComponent, TextComponent>()) {
			if (textComp) {
				continue;
			}

			Engine::instance()->rhi()->setUniform(pipeline, "drawColor", color->color);
			
			auto pos = position->pos;
			pos.x += -position->pivot.x * position->size.x;

			pos.y = Engine::instance()->getWindow()->getScreenData().height - pos.y;
			pos.y -= position->size.y;
			pos.y += position->pivot.y * position->size.y;

			const float startX = (pos.x - semiW) / semiW;
			const float startY = (pos.y - semiH) / semiH;

			const float endX = (pos.x + position->size.x - semiW) / semiW;
			const float endY = (pos.y + position->size.y - semiH) / semiH;

			const Math::Vec2 vertices[6] = {
				 {startX,	endY},
				 {startX,	startY},
				 {endX,		startY},

				 {startX,	endY},
				 {endX,		startY},
				 {endX,		endY},
			};
			Engine::instance()->rhi()->updateBuffer(VBO, i * sizeof(vertices), sizeof(vertices), vertices);
			i++;
		}

		Engine::instance()->rhi()->draw(i * 6);


		for (auto [entId, textComp, fontComp, position, color] : registry.view<TextComponent, FontComponent, PosComponent, ColorComponent>()) {
			TextRenderer::instance()->renderText(textComp->text, position->pos.x + -position->pivot.x * position->size.x, position->pos.y + (1.f - position->pivot.y) * position->size.y, textComp->scale, color->color, fontComp->font);
		}
	}
}

