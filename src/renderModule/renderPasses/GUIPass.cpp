#include "GUIPass.h"

#include "mathModule/Utils.h"

namespace SFE::Render::RenderPasses {
	GUIPass::~GUIPass() {}

	void GUIPass::init(SystemsModule::RenderData& renderDataHandle) {
	
		
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

		Render::DescriptorSetLayoutDesc texLayoutDesc;
		texLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);

		mTexSetLayout = Engine::instance()->rhi()->createDescriptorSetLayout(texLayoutDesc);
		mTexSet = Engine::instance()->rhi()->allocateDescriptorSet(mTexSetLayout);

		BufferDesc drawDataUBODesc;
		drawDataUBODesc.size = sizeof(Math::Mat4);
		drawDataUBODesc.usage = BufferUsage::Uniform;
		drawDataUBODesc.cpuAccess = CpuAccess::Write;
		auto proj = Math::orthoRH_NO(0.0f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().width), 0.f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().height), -1.f, 1.f);
		drawDataUBO = Engine::instance()->rhi()->createBuffer(drawDataUBODesc, &proj);

		Engine::instance()->rhi()->updateDescriptorSets({
			{
				.set = mTexSet,
				.binding = 1,
				.type = Render::DescriptorType::UniformBuffer,
				.buffers = {{drawDataUBO}}
			}
			});
	}

	void GUIPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		if (registry.getAllEntities().empty()) {
			return;
		}
		
		Engine::instance()->rhi()->cmdBindPipeline(cmd, pipeline);
		Engine::instance()->rhi()->cmdBindVertexBuffer(cmd, 0, VBO);
		Engine::instance()->rhi()->cmdBindDescriptorSets(cmd, pipeline, 0, &mTexSet, 1);

		const float semiW = Engine::instance()->getWindow()->getScreenData().width * 0.5f;
		const float semiH = Engine::instance()->getWindow()->getScreenData().height * 0.5f;
		int i = 0;
		for (auto [entId, position, color, textComp] : registry.view<PosComponent, ColorComponent, TextComponent>()) {
			if (textComp) {
				continue;
			}
			Engine::instance()->rhi()->cmdPushConstants(cmd, pipeline, color->color);
			
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

		Engine::instance()->rhi()->cmdDraw(cmd, i * 6);


		for (auto [entId, textComp, fontComp, position, color] : registry.view<TextComponent, FontComponent, PosComponent, ColorComponent>()) {
			TextRenderer::instance()->renderText(cmd, textComp->text, position->pos.x + -position->pivot.x * position->size.x, position->pos.y + (1.f - position->pivot.y) * position->size.y, textComp->scale, color->color, fontComp->font);
		}
	}
}

