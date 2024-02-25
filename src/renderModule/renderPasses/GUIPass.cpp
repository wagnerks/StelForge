#include "GUIPass.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "mathModule/Utils.h"
#include "renderModule/Renderer.h"

namespace SFE::Render::RenderPasses {
	GUIPass::~GUIPass() {}

	void GUIPass::init() {
		const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/2dshader.vs", "shaders/2dshader.fs");

		shader->use();
		shader->setMat4("projection", Math::orthoRH_NO(0.0f, static_cast<float>(Renderer::SCR_WIDTH), 0.f, static_cast<float>(Renderer::SCR_HEIGHT), -1.f, 1.f));
		VAO.generate();
		VAO.bind();
		VBO.bind();
		//VBO.allocateData(sizeof(float) * 6 * 2, 1, DYNAMIC_DRAW);

		VAO.addAttribute(0, 2, FLOAT, false, 2 * sizeof(float));
		VAO.bindDefault();
		VBO.unbind();
	}

	void GUIPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
		if (registry.getAllEntities().empty()) {
			return;
		}
		
		const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/2dshader.vs", "shaders/2dshader.fs");
		shader->use();

		VAO.bind();

		Render::CapabilitiesStack::push(Render::BLEND, true);
		VBO.bind();
		VBO.allocateData(sizeof(float) * 2 * 3 * 2, registry.getAllEntities().size(), DYNAMIC_DRAW);

		const float semiW = Render::Renderer::SCR_WIDTH * 0.5f;
		const float semiH = Render::Renderer::SCR_HEIGHT * 0.5f;
		int i = 0;
		for (auto [entId, position, color, textComp] : registry.getComponentsArray<PosComponent, ColorComponent, TextComponent>()) {
			if (textComp) {
				continue;
			}
			shader->setVec4("drawColor", color->color);
			
			auto pos = position->pos;
			pos.x += -position->pivot.x * position->size.x;

			pos.y = Render::Renderer::SCR_HEIGHT - pos.y;
			pos.y -= position->size.y;
			pos.y += position->pivot.y * position->size.y;

			const float startX = (pos.x - semiW) / semiW;
			const float startY = (pos.y - semiH) / semiH;

			const float endX = (pos.x + position->size.x - semiW) / semiW;
			const float endY = (pos.y + position->size.y - semiH) / semiH;

			const float vertices[12] = {
				 startX,	endY,
				 startX,	startY,
				 endX,		startY,

				 startX,	endY,
				 endX,		startY,
				 endX,		endY,
			};

			VBO.setData(1, vertices, i);
			i++;
		}

        Render::Renderer::drawArrays(TRIANGLES, i * 6);
		Render::CapabilitiesStack::pop();
		VBO.unbind();
		VAO.bindDefault();

		for (auto [entId, textComp, fontComp, position, color] : registry.getComponentsArray<TextComponent, FontComponent, PosComponent, ColorComponent>()) {
			TextRenderer::instance()->renderText(textComp->text, position->pos.x + -position->pivot.x * position->size.x, position->pos.y + (1.f - position->pivot.y) * position->size.y, textComp->scale, color->color, fontComp->font);
		}
	}
}

