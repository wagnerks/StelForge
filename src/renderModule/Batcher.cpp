#include "Batcher.h"

#include <algorithm>

#include "assetsModule/TextureHandler.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/TransformComponent.h"
#include "debugModule/Benchmark.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/Draw.h"
#include "systemsModule/systems/CameraSystem.h"

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	std::ranges::sort(matrices, [&viewPos](const Matrices& a, const Matrices& b) {//todo sort bones too
		return SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*a.transform)[3])) < SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*b.transform)[3]));
	});
}

void Batcher::addToDrawList(unsigned VAO, size_t vertices, size_t indices, const SFE::ComponentsModule::Materials& textures, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms) {
	if (!vertices) {
		return;
	}
	auto it = drawList.find(VAO);
	if (it != drawList.end()) {
		it->second.matrices.emplace_back(const_cast<SFE::Math::Mat4*>(&transform), bonesTransforms);
	}
	else {
		auto& obj  = drawList[VAO];
		obj.VAO = VAO;
		obj.verticesCount = vertices;
		obj.indicesCount = indices;
		obj.materialData = textures;
		obj.matrices.reserve(1000000);
		obj.matrices.emplace_back(const_cast<SFE::Math::Mat4*>(&transform), bonesTransforms);
		if (bonesTransforms) {
			obj.bonesCount++;
		}
		drawList.insert({ VAO, obj });
	}
}

void Batcher::sort(const SFE::Math::Vec3& viewPos) {
	for (auto& drawObjects : drawList) {
		drawObjects.second.sortTransformAccordingToView(viewPos);
	}

	/*drawList.sort([&viewPos](const std::shared_ptr<DrawObject>& a, const std::shared_ptr<DrawObject>& b) {
		return SFE::Math::distanceSqr(viewPos, (*a.get()->matrices.front().transform)[3].xyz) > SFE::Math::distanceSqr(viewPos, (*b.get()->matrices.front().transform)[3].xyz);
	});*/
}

void Batcher::flushAll() {
	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");
	int i = 0;
	for (auto& drawObjects : drawList) {
		auto objs = drawObjects.second;
		SFE::Debug::BenchmarkFunc scopeObj = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__) + std::string(" flush all") + std::to_string(i++));

		SFE::GLW::VertexArray::bindArray(objs.VAO);
		SFE::GLW::Buffers<2> batcherBuffer{SFE::GLW::SHADER_STORAGE_BUFFER};

		{
			FUNCTION_BENCHMARK_NAMED(transforms_binding)
			batcherBuffer.bind(0);
			batcherBuffer.setBufferBinding(1, 0);
			batcherBuffer.allocateData<SFE::Math::Mat4>(objs.matrices.size(), SFE::GLW::DYNAMIC_DRAW);
			std::vector<SFE::Math::Mat4> transforms;
			transforms.reserve(objs.matrices.size());
			for (auto j = 0u; j < objs.matrices.size(); j++) {
				transforms.emplace_back(*objs.matrices[j].transform);
			}
			//static void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, transforms.size() * sizeof(SFE::Math::Mat4), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
			//memcpy(ptr, transforms.data(), transforms.size() * sizeof(SFE::Math::Mat4));
			//glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			batcherBuffer.setData<SFE::Math::Mat4>(transforms.size(), transforms.data());
		}

		if (objs.bonesCount) {
			FUNCTION_BENCHMARK_NAMED(bones_binding)
			batcherBuffer.bind(1);
			batcherBuffer.setBufferBinding(2, 1);
			batcherBuffer.allocateData(sizeof(SFE::Math::Mat4) * 100, objs.matrices.size(), SFE::GLW::DYNAMIC_DRAW);
			auto size = sizeof(SFE::Math::Mat4) * 100;
			for (auto j = 0; j < objs.matrices.size(); j++) {
				if (objs.matrices[j].bones) {
					batcherBuffer.setData(size, 1, objs.matrices[j].bones, j);
				}
				else {
					batcherBuffer.setData(size, 1, mBones, j);
				}
			}
		}
		
		
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::DIFFUSE, defaultTex);
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::NORMALS, defaultNormal);
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::SPECULAR, defaultTex);

		for (auto i = 0; i < objs.materialData.materialsCount; i++) {
			const auto& mat = objs.materialData.material[i];
			SFE::GLW::bindTextureToSlot(mat.slot, mat.type, mat.textureId);
		}

		SFE::GLW::drawVerticesW(SFE::GLW::TRIANGLES, objs.verticesCount, objs.indicesCount, objs.matrices.size());
	}
	SFE::GLW::Buffer::bindDefaultBuffer(SFE::GLW::SHADER_STORAGE_BUFFER);
	SFE::GLW::VertexArray::bindDefault();
}

void Batcher::clear() {
	drawList.clear();
}
