#include "GUIPass.h"

#include "core/Engine.h"
#include "mathModule/Utils.h"
#include "renderModule/FrameBlackboard.h"
#include "systemsModule/systems/RenderSystem.h"
#include "windowsModule/Window.h"

#include <vector>

namespace SFE::Render::RenderPasses {
	GUIPass::~GUIPass() {}

	void GUIPass::init(SystemsModule::RenderData&) {
		auto* rhi = Engine::instance()->rhi();

		BufferDesc vboDesc;
		vboDesc.size = sizeof(Math::Vec2) * 6 * 1000;
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		VBO = rhi->createBuffer(vboDesc);

		RenderPassDesc rpd;
		rpd.colorAttachmentCount = 1;
		rpd.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		rpd.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		rpd.colorAttachments[0].format = TextureFormat::RGBA8;
		mRenderPass = rhi->createRenderPass(rpd);

		DescriptorSetLayoutDesc texLayoutDesc;
		texLayoutDesc.bindings.emplace_back(1, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment);
		mTexSetLayout = rhi->createDescriptorSetLayout(texLayoutDesc);
		mTexSet = rhi->allocateDescriptorSet(mTexSetLayout);

		BufferDesc drawDataUBODesc;
		drawDataUBODesc.size = sizeof(Math::Mat4);
		drawDataUBODesc.usage = BufferUsage::Uniform;
		drawDataUBODesc.cpuAccess = CpuAccess::Write;
		const auto proj = Math::orthoRH_NO(0.0f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().width), 0.f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().height), -1.f, 1.f);
		drawDataUBO = rhi->createBuffer(drawDataUBODesc, &proj);

		rhi->updateDescriptorSets({
			{
				.set = mTexSet,
				.binding = 1,
				.type = DescriptorType::UniformBuffer,
				.buffers = {{drawDataUBO}}
			}
			});

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/2dshader.vs");
		pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/2dshader.fs");
		pipelineDesc.topology = PrimitiveTopology::Triangles;
		pipelineDesc.cullMode = CullMode::None;
		pipelineDesc.blend = { true, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };
		pipelineDesc.depthStencilState = { false };
		pipelineDesc.renderPass = mRenderPass;
		pipelineDesc.descriptorSetLayouts = { mTexSetLayout };
		pipelineDesc.pushConstantSize = static_cast<uint32_t>(sizeof(Math::Vec4));
		pipelineDesc.pushConstantStages = ShaderStage::Fragment;
		pipelineDesc.layout.bindings.push_back(VertexBindingDesc{ 0, sizeof(Math::Vec2), VertexInputRate::PerVertex });
		pipelineDesc.layout.attributes.push_back(VertexAttributeDesc{
			.location = 0,
			.binding = 0,
			.format = VertexFormat::Float2,
			.offset = 0,
			.normalized = false
			});
		pipeline = rhi->createGraphicsPipeline(pipelineDesc);
	}

	void GUIPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		if (registry.getAllEntities().empty()) {
			return;
		}

		auto* rhi = Engine::instance()->rhi();
		const auto fb = renderDataHandle.frame.framebuffer(FrameId::Final);
		if (!fb || !mRenderPass) {
			return;
		}

		constexpr uint32_t kMaxQuads = 1000;
		std::vector<Math::Vec2> vertices;
		std::vector<Math::Vec4> colors;
		vertices.reserve(64);
		colors.reserve(16);

		const float semiW = Engine::instance()->getWindow()->getScreenData().width * 0.5f;
		const float semiH = Engine::instance()->getWindow()->getScreenData().height * 0.5f;
		for (auto [entId, position, color, textComp] : registry.view<PosComponent, ColorComponent, TextComponent>()) {
			if (textComp) {
				continue;
			}
			if (colors.size() >= kMaxQuads) {
				break;
			}

			auto pos = position->pos;
			pos.x += -position->pivot.x * position->size.x;

			pos.y = Engine::instance()->getWindow()->getScreenData().height - pos.y;
			pos.y -= position->size.y;
			pos.y += position->pivot.y * position->size.y;

			const float startX = (pos.x - semiW) / semiW;
			const float startY = (pos.y - semiH) / semiH;
			const float endX = (pos.x + position->size.x - semiW) / semiW;
			const float endY = (pos.y + position->size.y - semiH) / semiH;

			vertices.push_back({ startX, endY });
			vertices.push_back({ startX, startY });
			vertices.push_back({ endX, startY });
			vertices.push_back({ startX, endY });
			vertices.push_back({ endX, startY });
			vertices.push_back({ endX, endY });
			colors.push_back(color->color);
		}

		if (!vertices.empty()) {
			rhi->cmdUpdateBuffer(cmd, VBO, 0, vertices.size() * sizeof(Math::Vec2), vertices.data());
		}

		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.renderPass = mRenderPass;
		rpBeginInfo.framebuffer = fb;
		rpBeginInfo.width = screen.renderW;
		rpBeginInfo.height = screen.renderH;
		rhi->cmdBeginRenderPass(cmd, rpBeginInfo);

		if (!colors.empty()) {
			rhi->cmdBindPipeline(cmd, pipeline);
			rhi->cmdBindVertexBuffer(cmd, 0, VBO);
			rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &mTexSet, 1);
			for (uint32_t i = 0; i < static_cast<uint32_t>(colors.size()); ++i) {
				rhi->cmdPushConstants(cmd, pipeline, ShaderStage::Fragment, &colors[i], sizeof(Math::Vec4));
				rhi->cmdDraw(cmd, 6, 1, i * 6, 0);
			}
		}

		for (auto [entId, textComp, fontComp, position, color] : registry.view<TextComponent, FontComponent, PosComponent, ColorComponent>()) {
			TextRenderer::instance()->renderText(cmd, textComp->text, position->pos.x + -position->pivot.x * position->size.x, position->pos.y + (1.f - position->pivot.y) * position->size.y, textComp->scale, color->color, fontComp->font);
		}

		rhi->cmdEndRenderPass(cmd);
	}
}
