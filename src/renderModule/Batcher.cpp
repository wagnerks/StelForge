#include "Batcher.h"

#include <algorithm>

#include "Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/TransformComponent.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/Draw.h"
#include "systemsModule/systems/CameraSystem.h"

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	std::ranges::sort(transforms, [&viewPos](const SFE::Math::Mat4& a, const SFE::Math::Mat4& b) {//todo sort bones too
		return SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3(a[3])) < SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3(b[3]));
	});
}

void Batcher::addToDrawList(unsigned VAO, size_t vertices, size_t indices, const SFE::ComponentsModule::Materials& textures, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms) {
	const auto drawObj = drawList.findReverse([VAO, maxDrawSize = maxDrawSize](const DrawObject& obj) {
		return obj == VAO && obj.transforms.size() < maxDrawSize;
	});

	if (drawObj) {
		drawObj->transforms.emplace_back(transform);
		drawObj->bones.emplace_back(bonesTransforms);
	}
	else {
		drawList.emplace_back();
		drawList.back().VAO = VAO;
		drawList.back().verticesCount = vertices;
		drawList.back().indicesCount = indices;
		drawList.back().materialData = textures;

		drawList.back().transforms.emplace_back(transform);
		drawList.back().bones.emplace_back(bonesTransforms);
	}
}

void Batcher::sort(const SFE::Math::Vec3& viewPos) {
	for (auto& drawObjects : drawList) {
		drawObjects.sortTransformAccordingToView(viewPos);
	}

	drawList.sort([&viewPos](const DrawObject& a, const DrawObject& b) {
		return SFE::Math::distanceSqr(viewPos, a.transforms.front()[3].xyz) > SFE::Math::distanceSqr(viewPos, b.transforms.front()[3].xyz);
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

		AssetsModule::TextureHandler::bindTextureToSlot(SFE::DIFFUSE, defaultTex);
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::NORMALS, defaultNormal);
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::SPECULAR, defaultTex);

		for (auto i = 0; i < drawObjects.materialData.materialsCount; i++) {
			const auto& mat = drawObjects.materialData.material[i];
			SFE::GLW::bindTextureToSlot(mat.slot, mat.type, mat.textureId);
		}
		

		SFE::GLW::drawVertices(SFE::GLW::TRIANGLES, drawObjects.VAO, drawObjects.verticesCount, drawObjects.indicesCount, drawObjects.transforms.size());
	}
	SFE::GLW::Buffer::bindDefaultBuffer(SFE::GLW::SHADER_STORAGE_BUFFER);
	SFE::GLW::VertexArray::bindDefault();

	if (clear) {
		drawList.clear();
	}
}
