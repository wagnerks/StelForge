#include "Batcher.h"

#include <algorithm>

#include "imgui.h"
#include "Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "glad/glad.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "systemsModule/systems/CameraSystem.h"

void DrawObject::sortTransformAccordingToView(const Engine::Math::Vec3& viewPos) {
	if (sortedPos == viewPos) {
		return;
	}
	sortedPos = viewPos;
	std::ranges::sort(transforms, [&viewPos](const Engine::Math::Mat4& a, const Engine::Math::Mat4& b) {
		return Engine::Math::distanceSqr(viewPos, Engine::Math::Vec3(a[3])) < Engine::Math::distanceSqr(viewPos, Engine::Math::Vec3(b[3]));
	});
}

Batcher::Batcher() {
}

void Batcher::addToDrawList(unsigned VAO, size_t vertices, size_t indices, const AssetsModule::Material& textures, const Engine::Math::Mat4& transform, bool transparentForShadow) {
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

void Batcher::sort(const Engine::Math::Vec3& viewPos) {
	for (auto& drawObjects : drawList) {
		drawObjects.sortTransformAccordingToView(viewPos);
	}

	std::ranges::sort(drawList, [&viewPos](const DrawObject& a, const DrawObject& b) {
		auto apos = Engine::Math::Vec3(a.transforms.front()[3]);
		auto bpos = Engine::Math::Vec3(b.transforms.front()[3]);

		return Engine::Math::distanceSqr(viewPos, apos) > Engine::Math::distanceSqr(viewPos, bpos);
	});
}

void Batcher::flushAll(bool clear) {
	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png")->mId;
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png")->mId;

	for (auto& drawObjects : drawList) {
		glBindVertexArray(drawObjects.VAO);
        
        glGenBuffers(1, &drawObjects.transformsBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, drawObjects.transformsBuffer);
        glBufferData(GL_UNIFORM_BUFFER,  sizeof(Engine::Math::Mat4) * drawObjects.transforms.size(), drawObjects.transforms.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, drawObjects.transformsBuffer);



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

		glDeleteBuffers(1, &drawObjects.transformsBuffer);
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindVertexArray(0);

	if (clear) {
		drawList.clear();
	}
}
