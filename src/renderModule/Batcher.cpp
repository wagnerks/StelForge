#include "Batcher.h"

#include <algorithm>

#include "imgui.h"
#include "Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "glad/glad.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "mathModule/MathUtils.h"
#include "systemsModule/CameraSystem.h"

void DrawObject::sortTransformAccordingToView(const glm::vec3& viewPos) {
	if (sortedPos == viewPos) {
		return;
	}
	sortedPos = viewPos;
	std::ranges::sort(transforms, [&viewPos](const glm::mat4& a, const glm::mat4& b) {
		return Engine::Math::distanceSqr(viewPos, glm::vec3(a[3])) < Engine::Math::distanceSqr(viewPos, glm::vec3(b[3]));
	});
}

Batcher::Batcher() {
	glGenBuffers(1, &ssboModelMatrices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelMatrices);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * maxDrawSize, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboModelMatrices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Batcher::addToDrawList(unsigned VAO, size_t vertices, size_t indices, const AssetsModule::Material& textures, const glm::mat4& transform, bool transparentForShadow) {
	auto it = std::find_if(drawList.rbegin(), drawList.rend(), [transparentForShadow, VAO, maxDrawSize = maxDrawSize](const DrawObject& obj) {
		return obj == VAO && obj.transforms.size() < maxDrawSize && obj.transparentForShadow == transparentForShadow;
	});

	if (it == drawList.rend()) {
		drawList.emplace_back();
		drawList.back().VAO = VAO;
		drawList.back().verticesCount = vertices;
		drawList.back().indicesCount = indices;
		drawList.back().material = textures;
		drawList.back().transforms.emplace_back(transform);
		drawList.back().transparentForShadow = transparentForShadow;
	}
	else {
		it->transforms.emplace_back(transform);

	}
}

void Batcher::flushAll(bool clear, const glm::vec3& viewPos) {
	for (auto& drawObjects : drawList) {
		drawObjects.sortTransformAccordingToView(viewPos);
	}

	std::ranges::sort(drawList, [&viewPos](const DrawObject& a, const DrawObject& b) {
		auto apos = glm::vec3(a.transforms.front()[3]);
		auto bpos = glm::vec3(b.transforms.front()[3]);

		return Engine::Math::distanceSqr(viewPos, apos) < Engine::Math::distanceSqr(viewPos, bpos);
	});

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelMatrices);
	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png")->mId;
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png")->mId;
	for (auto& drawObjects : drawList) {
		glBindVertexArray(drawObjects.VAO);

		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::mat4x4) * drawObjects.transforms.size(), &drawObjects.transforms[0]);


		if (drawObjects.material.mDiffuse.mTexture->isValid()) {
			AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, drawObjects.material.mDiffuse.mTexture->mId);
		}
		else {
			AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, defaultTex);
		}

		if (drawObjects.material.mNormal.mTexture->isValid()) {
			AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, drawObjects.material.mNormal.mTexture->mId);
		}
		else {
			AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, defaultNormal);
		}

		if (drawObjects.indicesCount) {
			Engine::RenderModule::Renderer::drawElementsInstanced(GL_TRIANGLES, static_cast<int>(drawObjects.indicesCount), GL_UNSIGNED_INT, static_cast<int>(drawObjects.transforms.size()));
		}
		else {
			Engine::RenderModule::Renderer::drawArraysInstancing(GL_TRIANGLES, static_cast<int>(drawObjects.verticesCount), static_cast<int>(drawObjects.transforms.size()));
		}
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindVertexArray(0);


	if (clear) {
		drawList.clear();
	}
}
