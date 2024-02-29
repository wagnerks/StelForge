#include "Batcher.h"

#include <algorithm>

#include "imgui.h"
#include "Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "glWrapper/Draw.h"
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
	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");

	for (auto& drawObjects : drawList) {
		SFE::GLW::VertexArray::bindArray(drawObjects.VAO);
		SFE::GLW::Buffers<2> batcherBuffer{SFE::GLW::SHADER_STORAGE_BUFFER};

		batcherBuffer.bind(0);
		batcherBuffer.setBufferBinding(1, 0);
		batcherBuffer.allocateData(drawObjects.transforms, SFE::GLW::DYNAMIC_DRAW);

		batcherBuffer.bind(1);
		batcherBuffer.setBufferBinding(2, 1);
		batcherBuffer.allocateData(drawObjects.bones, SFE::GLW::DYNAMIC_DRAW);

		if (auto texture = drawObjects.material.tryGetTexture(AssetsModule::DIFFUSE); texture && texture->isValid()) {
			AssetsModule::TextureHandler::bindTextureToSlot(AssetsModule::DIFFUSE, texture);
		}
		else {
			AssetsModule::TextureHandler::bindTextureToSlot(AssetsModule::DIFFUSE, defaultTex);
		}

		if (auto texture = drawObjects.material.tryGetTexture(AssetsModule::NORMALS);  texture && texture->isValid()) {
			AssetsModule::TextureHandler::bindTextureToSlot(AssetsModule::NORMALS, texture);
		}
		else {
			AssetsModule::TextureHandler::bindTextureToSlot(AssetsModule::NORMALS, defaultNormal);
		}

		if (auto texture = drawObjects.material.tryGetTexture(AssetsModule::SPECULAR);  texture && texture->isValid()) {
			AssetsModule::TextureHandler::bindTextureToSlot(AssetsModule::SPECULAR, texture);
		}
		else {
			AssetsModule::TextureHandler::bindTextureToSlot(AssetsModule::SPECULAR, defaultTex);
		}

		SFE::GLW::drawVertices(SFE::GLW::TRIANGLES, drawObjects.VAO, drawObjects.verticesCount, drawObjects.indicesCount, drawObjects.transforms.size());
	}
	SFE::GLW::Buffer::bindDefaultBuffer(SFE::GLW::SHADER_STORAGE_BUFFER);
	SFE::GLW::VertexArray::bindDefault();

	if (clear) {
		drawList.clear();
	}
}
