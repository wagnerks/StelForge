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

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	if (sortedPos == viewPos) {
		return;
	}
	sortedPos = viewPos;
	std::ranges::sort(transforms, [&viewPos](const SFE::Math::Mat4& a, const SFE::Math::Mat4& b) {
		return SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3(a[3])) < SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3(b[3]));
	});
}

Batcher::Batcher() {
}

void Batcher::addToDrawList(unsigned VAO, size_t vertices, size_t indices, const AssetsModule::Material& textures, const SFE::Math::Mat4& transform, const std::vector<SFE::Math::Mat4>& bonesTransforms, bool transparentForShadow) {
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
		drawList.back().bones.emplace_back(bonesTransforms);
	}
	else {
		it->transforms.emplace_back(transform);
		it->bones.emplace_back(bonesTransforms);
	}
}

void Batcher::sort(const SFE::Math::Vec3& viewPos) {
	for (auto& drawObjects : drawList) {
		drawObjects.sortTransformAccordingToView(viewPos);
	}

	std::ranges::sort(drawList, [&viewPos](const DrawObject& a, const DrawObject& b) {
		auto apos = SFE::Math::Vec3(a.transforms.front()[3]);
		auto bpos = SFE::Math::Vec3(b.transforms.front()[3]);

		return SFE::Math::distanceSqr(viewPos, apos) > SFE::Math::distanceSqr(viewPos, bpos);
	});
}

void Batcher::flushAll(bool clear) {
	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png")->mId;
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png")->mId;

	for (auto& drawObjects : drawList) {
		glBindVertexArray(drawObjects.VAO);
		
		glGenBuffers(2, drawObjects.batcherBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawObjects.batcherBuffer[0]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, drawObjects.batcherBuffer[0]);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(SFE::Math::Mat4) * drawObjects.transforms.size(), drawObjects.transforms.data(), GL_DYNAMIC_DRAW);
		
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, drawObjects.batcherBuffer[1]);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, drawObjects.batcherBuffer[1]);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(BonesData) * drawObjects.bones.size(), drawObjects.bones.data(), GL_DYNAMIC_DRAW);

		if (auto texture = drawObjects.material.tryGetTexture(AssetsModule::DIFFUSE); texture && texture->isValid()) {
			AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::DIFFUSE, AssetsModule::TEXTURE_2D, texture->mId);
		}
		else {
			//AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::DIFFUSE, AssetsModule::TEXTURE_2D, 0);
			AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::DIFFUSE, AssetsModule::TEXTURE_2D, defaultTex);
		}

		if (auto texture = drawObjects.material.tryGetTexture(AssetsModule::NORMALS);  texture && texture->isValid()) {
			AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::NORMALS, AssetsModule::TEXTURE_2D, texture->mId);
		}
		else {
			AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::NORMALS, AssetsModule::TEXTURE_2D, defaultNormal);
		}

		if (auto texture = drawObjects.material.tryGetTexture(AssetsModule::SPECULAR);  texture && texture->isValid()) {
			AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::SPECULAR, AssetsModule::TEXTURE_2D, texture->mId);
		}
		else {
			AssetsModule::TextureHandler::instance()->bindTextureToSlot(AssetsModule::SPECULAR, AssetsModule::TEXTURE_2D, defaultTex);
		}

		SFE::Render::Renderer::drawVertices(SFE::Render::TRIANGLES, drawObjects.VAO, drawObjects.verticesCount, drawObjects.indicesCount, drawObjects.transforms.size());

		glDeleteBuffers(2, drawObjects.batcherBuffer);
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindVertexArray(0);

	if (clear) {
		drawList.clear();
	}
}
