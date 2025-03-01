﻿#include "GUIPass.h"

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
		VAO.generate();
		VAO.bind();
		VBO.generate();
		VBO.bind();
		VAO.addAttribute(0, &GUIPass::Vertex::pos, false);
		VAO.bindDefault();
		VBO.unbind();
	}

	void GUIPass::render(SystemsModule::RenderData& renderDataHandle) {
		if (registry.getAllEntities().empty()) {
			return;
		}
		GLW::CapabilitiesStack<GLW::DEPTH_TEST>::push(false);

		const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/2dshader.vs", "shaders/2dshader.fs");
		shader->use();
		

		VBO.bind();
		VBO.clear();
		VBO.shrinkToFit();
		VBO.reserve(registry.getAllEntities().size() * 6);

		const float semiW = Engine::instance()->getWindow()->getScreenData().width * 0.5f;
		const float semiH = Engine::instance()->getWindow()->getScreenData().height * 0.5f;
		int i = 0;
		for (auto [entId, position, color, textComp] : registry.forEach<PosComponent, ColorComponent, TextComponent>()) {
			if (textComp) {
				continue;
			}
			shader->setUniform("drawColor", color->color);

			auto pos = position->pos;
			pos.x += -position->pivot.x * position->size.x;

			pos.y = Engine::instance()->getWindow()->getScreenData().height - pos.y;
			pos.y -= position->size.y;
			pos.y += position->pivot.y * position->size.y;

			const float startX = (pos.x - semiW) / semiW;
			const float startY = (pos.y - semiH) / semiH;

			const float endX = (pos.x + position->size.x - semiW) / semiW;
			const float endY = (pos.y + position->size.y - semiH) / semiH;

			const Vertex vertices[6] = {
				{{startX, endY}},
				{{startX, startY}},
				{{endX, startY}},

				{{startX, endY}},
				{{endX, startY}},
				{{endX, endY}},
			};

			VBO.addData(6, vertices);

			i++;
		}
		VBO.unbind();

		GLW::CapabilitiesStack<GLW::BLEND>::push(true);
		GLW::BlendFuncStack::push({ GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });
		GLW::drawVertices(GLW::TRIANGLES, VAO.getID(), i * 6);
		GLW::BlendFuncStack::pop();
		GLW::CapabilitiesStack<GLW::BLEND>::pop();

		for (auto [entId, textComp, fontComp, position, color] : registry.forEach<TextComponent, FontComponent, PosComponent, ColorComponent>()) {
			TextRenderer::instance()->renderText(textComp->text, position->pos.x + -position->pivot.x * position->size.x, position->pos.y + (1.f - position->pivot.y) * position->size.y, textComp->scale, color->color, fontComp->font);
		}

		GLW::CapabilitiesStack<GLW::DEPTH_TEST>::pop();
	}
}

