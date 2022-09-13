#include "Batcher.h"

#include <algorithm>

#include "imgui.h"
#include "Renderer.h"
#include "TextureHandler.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "glad/glad.h"
#include "shaderModule/ShaderController.h"
#include "mathModule/MathUtils.h"

void DrawObject::sortTransformAccordingToView(const glm::vec3& viewPos) {
	if (sortedPos == viewPos) {
		return;	
	}
	sortedPos = viewPos;
	std::ranges::sort(transforms, [&viewPos](const glm::mat4& a, const glm::mat4& b) {
		return GameEngine::Math::distanceSqr(viewPos, glm::vec3(a[3])) < GameEngine::Math::distanceSqr(viewPos, glm::vec3(b[3]));
	});
}

Batcher::Batcher() {
	glGenBuffers(1, &ssboModelMatrices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelMatrices);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * maxDrawSize, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboModelMatrices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Batcher::addToDrawList(unsigned VAO, size_t vertices, size_t indices, GameEngine::ModelModule::Material textures, glm::mat4 transform, bool transparentForShadow) {
	auto apos = glm::vec3(transform[3]);
	if (GameEngine::Math::distanceSqr(apos, GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getPos()) > 500000.f * 500000.f) {
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

			return GameEngine::Math::distanceSqr(cameraPos, apos) < GameEngine::Math::distanceSqr(cameraPos, bpos);
			
		}

		return GameEngine::Math::distanceSqr(viewPos, apos) < GameEngine::Math::distanceSqr(viewPos, bpos);
	});

	ImGui::Begin("draw order");
	for (auto& drawObjects : drawList) {
		ImGui::Text("%d", drawObjects.indicesCount);
	}
	ImGui::End();

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelMatrices);
	auto defaultTex = GameEngine::RenderModule::TextureHandler::getInstance()->mLoader.loadTexture("white.png").mId;
	for (auto& drawObjects : drawList) {
		if (shadowMap) {
			if (drawObjects.transparentForShadow) {
				continue;
			}
		}
		glBindVertexArray(drawObjects.VAO);

		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::mat4x4) * drawObjects.transforms.size(), &drawObjects.transforms[0]);


		if (drawObjects.material.mDiffuse.mTexture.isValid()) {
			GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, drawObjects.material.mDiffuse.mTexture.mId);
		}
		else {
			GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, defaultTex);
		}

		if (drawObjects.material.mNormal.mTexture.isValid()) {
			GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, drawObjects.material.mNormal.mTexture.mId);
		}
		else {
			GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, defaultTex);
		}

		if (drawObjects.indicesCount) {
			GameEngine::RenderModule::Renderer::drawElementsInstanced(GL_TRIANGLES, static_cast<int>(drawObjects.indicesCount), GL_UNSIGNED_INT, static_cast<int>(drawObjects.transforms.size()));
		}
		else {
			GameEngine::RenderModule::Renderer::drawArraysInstancing(GL_TRIANGLES, static_cast<int>(drawObjects.verticesCount), static_cast<int>(drawObjects.transforms.size()));
		}
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindVertexArray(0);
	

	if (clear) {
		drawList.clear();
	}
}
