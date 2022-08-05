#include "Batcher.h"

#include <algorithm>

#include "imgui.h"
#include "Renderer.h"
#include "TextureHandler.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "glad/glad.h"

void DrawObject::sortTransformAccordingToView(const glm::vec3& viewPos) {
	if (sortedPos == viewPos) {
		return;	
	}
	sortedPos = viewPos;
	std::ranges::sort(transforms, [&viewPos](const glm::mat4& a, const glm::mat4& b) {
		return glm::distance(viewPos, glm::vec3(a[3])) < glm::distance(viewPos, glm::vec3(b[3]));
	});
}

Batcher::Batcher() {
	glGenBuffers(1, &ssboModelMatrices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelMatrices);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * maxDrawSize, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboModelMatrices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Batcher::addToDrawList(unsigned VAO, unsigned vertices, unsigned indices, std::vector<GameEngine::ModelModule::MeshTexture> textures, glm::mat4 transform, bool transparentForShadow) {
	auto apos = glm::vec3(transform[3]);
	if (glm::distance(apos, GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getPos()) > 1100.f) {
		return;
	}
	
	auto it = std::find_if(drawList.rbegin(), drawList.rend(), [transparentForShadow, VAO, maxDrawSize = maxDrawSize](const DrawObject& obj) {
		return obj == VAO && obj.transforms.size() < maxDrawSize && obj.transparentForShadow == transparentForShadow;
	});

	if (it == drawList.rend()) {
		drawList.push_back({VAO, vertices, indices, textures, {transform}, transparentForShadow});
	}
	else {
		it->transforms.push_back(transform);

	}
}

void Batcher::flushAll(bool clear, const glm::vec3& viewPos, bool shadowMap) {
	for (auto& drawObjects : drawList) {
		if (viewPos == glm::vec3{}) {
			drawObjects.sortTransformAccordingToView(GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getPos());
		}
		else {
			drawObjects.sortTransformAccordingToView(viewPos);
		}
	}

	std::ranges::sort(drawList, [&viewPos](const DrawObject& a, const DrawObject& b) {
		auto apos = glm::vec3(a.transforms.front()[3]);
		auto bpos = glm::vec3(b.transforms.front()[3]);
		if (viewPos == glm::vec3{}) {
			auto cameraPos = GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getPos();

			return glm::distance(cameraPos, apos) < glm::distance(cameraPos, bpos);
			
		}

		return glm::distance(viewPos, apos) < glm::distance(viewPos, bpos);
	});

	ImGui::Begin("draw order");
	for (auto& drawObjects : drawList) {
		ImGui::Text("%d", drawObjects.indicesCount);
	}
	ImGui::End();
	
	for (auto& drawObjects : drawList) {
		if (shadowMap) {
			if (drawObjects.transparentForShadow) {
				continue;
			}
		}
		glBindVertexArray(drawObjects.VAO);

		
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelMatrices);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::mat4x4) * drawObjects.transforms.size(), &drawObjects.transforms[0]);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		for (auto texture : drawObjects.textures) {
			if (texture.type == "texture_diffuse") {
				GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, texture.id);
			}
			else if (texture.type == "texture_normal") {
				GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, texture.id);
			}
		}

		GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, GameEngine::RenderModule::TextureHandler::getInstance()->loader.loadTexture("white.png"));

		if (drawObjects.indicesCount) {
			GameEngine::RenderModule::Renderer::drawElementsInstanced(GL_TRIANGLES, static_cast<int>(drawObjects.indicesCount), GL_UNSIGNED_INT, static_cast<int>(drawObjects.transforms.size()));
		}
		else {
			GameEngine::RenderModule::Renderer::drawArraysInstancing(GL_TRIANGLES, static_cast<int>(drawObjects.verticesCount), static_cast<int>(drawObjects.transforms.size()));
		}
	}


	glBindVertexArray(0);

	if (clear) {
		drawList.clear();
	}
}
