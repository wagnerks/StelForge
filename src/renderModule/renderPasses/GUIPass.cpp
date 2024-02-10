#include "GUIPass.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "mathModule/Utils.h"
#include "renderModule/Renderer.h"

namespace SFE::RenderModule::RenderPasses {
	GUIPass::~GUIPass() {
		glDeleteBuffers(1, &VBO);
		glDeleteVertexArrays(1, &VAO);
	}

	void GUIPass::init() {
		const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/2dshader.vs", "shaders/2dshader.fs");

		shader->use();
		shader->setMat4("projection", Math::orthoRH_NO(0.0f, static_cast<float>(Renderer::SCR_WIDTH), 0.f, static_cast<float>(Renderer::SCR_HEIGHT), -1.f, 1.f));

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void GUIPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
		if (registry.getAllEntities().empty()) {
			return;
		}
		
		const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/2dshader.vs", "shaders/2dshader.fs");
		shader->use();


        glBindVertexArray(VAO);
        glEnable(GL_BLEND);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * 3 * 2 * registry.getAllEntities().size(), nullptr, GL_DYNAMIC_DRAW);

		const float semiW = RenderModule::Renderer::SCR_WIDTH * 0.5f;
		const float semiH = RenderModule::Renderer::SCR_HEIGHT * 0.5f;
		int i = 0;
		for (auto [entId, position, color, textComp] : registry.getComponentsArray<PosComponent, ColorComponent, TextComponent>()) {
			if (textComp) {
				continue;
			}
			shader->setVec4("drawColor", color->color);
			
			auto pos = position->pos;
			pos.x += -position->pivot.x * position->size.x;

			pos.y = RenderModule::Renderer::SCR_HEIGHT - pos.y;
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

			glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices) * i, sizeof(vertices), vertices);

			i++;
		}

        RenderModule::Renderer::drawArrays(GL_TRIANGLES, i * 6);
        glDisable(GL_BLEND);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

		for (auto [entId, textComp, fontComp, position, color] : registry.getComponentsArray<TextComponent, FontComponent, PosComponent, ColorComponent>()) {
			TextRenderer::instance()->renderText(textComp->text, position->pos.x + -position->pivot.x * position->size.x, position->pos.y + (1.f - position->pivot.y) * position->size.y, textComp->scale, color->color, fontComp->font);
		}
	}
}

