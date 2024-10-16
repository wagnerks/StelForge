#include "Batcher.h"

#include <algorithm>

#include "imgui.h"
#include "Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/TransformComponent.h"
#include "debugModule/Benchmark.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/Draw.h"
#include "systemsModule/systems/CameraSystem.h"

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	std::unordered_map<ecss::EntityId, float> distanceCash;
	std::ranges::sort(entities, [&viewPos, this, &distanceCash](ecss::EntityId a, ecss::EntityId b) {
		auto aIt = distanceCash.find(a);
		if (aIt == distanceCash.end()){
			aIt = distanceCash.insert({ a, SFE::Math::distanceSqr(viewPos, (*transforms[a])[3].xyz) }).first;
		}

		auto bIt = distanceCash.find(b);
		if (bIt == distanceCash.end()) {
			bIt = distanceCash.insert({ b, SFE::Math::distanceSqr(viewPos, (*transforms[b])[3].xyz) }).first;
		}

		return aIt->second < bIt->second;
	});
}

void Batcher::addToDrawList(ecss::EntityId entity, unsigned VAO, size_t vertices, size_t indices, const SFE::ComponentsModule::Materials& textures, const SFE::Math::Mat4& transform) {
	if (!vertices) {
		return;
	}
	DrawObject* drawObj = nullptr;
	if (drawList.size()) {
		for (size_t i = drawList.size() - 1; i >= 0; i--) {
			if (drawList[i]->VAO == VAO && drawList[i]->entities.size() < maxDrawSize) {
				drawObj = drawList[i];
				break;
			}
			if (i == 0) {
				break;
			}
		}
	}

	auto id = static_cast<uint32_t>(DrawDataHolder::instance()->getEntityIdx(entity));
	if (drawObj) {
		drawObj->transforms.resize(id + 1);
		drawObj->transforms[id] = &transform;

		drawObj->entities.emplace_back(id);
	}
	else {
		drawList.emplace_back(new DrawObject{ VAO, vertices, indices, textures });
		drawList.back()->transforms.reserve(10000);
		drawList.back()->transforms.resize(id + 1);
		drawList.back()->transforms[id] = &transform;

		drawList.back()->entities.reserve(10000);
		drawList.back()->entities.emplace_back(id);
	}
}

void Batcher::sort(const SFE::Math::Vec3& viewPos) {
	for (auto drawObjects : drawList) {
		drawObjects->sortTransformAccordingToView(viewPos);
	}

	drawList.sort([&viewPos](DrawObject* a,DrawObject* b) {
		return SFE::Math::distanceSqr(viewPos, (*a->transforms[a->entities.front()])[3].xyz) > SFE::Math::distanceSqr(viewPos, (*b->transforms[b->entities.front()])[3].xyz);
	});
}

template<size_t Size>
struct BuffersRing {
	std::array<SFE::GLW::ArrayBuffer<unsigned int, SFE::GLW::DYNAMIC_DRAW>, Size> entityIds;
	std::array<GLsync, Size> fences { nullptr };

	int current = 0;
	bool inited = false;

	void init(size_t bufferReserveSize = 0) {
		if (inited) {
			return;
		}
		inited = true;

		for (auto& entitiyIdBuffer : entityIds) {
			entitiyIdBuffer.generate();
			entitiyIdBuffer.bind();
			entitiyIdBuffer.reserve(bufferReserveSize);
		}
	}

	SFE::GLW::ArrayBuffer<unsigned int, SFE::GLW::DYNAMIC_DRAW>& getBuffer() {
		FUNCTION_BENCHMARK;
		bool bufferAvailable = false;
		for (int i = 0; i < Size; i++) {
			// Check if the current buffer's fence is done (GPU has finished with it)
			if (fences[current]) {
				GLenum waitStatus = glClientWaitSync(fences[current], GL_SYNC_FLUSH_COMMANDS_BIT, 100000);
				if (waitStatus == GL_ALREADY_SIGNALED || waitStatus == GL_CONDITION_SATISFIED) {
					bufferAvailable = true;
					break;
				}
			}
			else {
				bufferAvailable = true;
				break;
			}

			current = (current + 1) % Size;
		}

		if (!bufferAvailable) {
			glWaitSync(fences[current], 0, GL_TIMEOUT_IGNORED);
		}

		return entityIds[current];
	}

	void rotate() {
		FUNCTION_BENCHMARK;
		if (fences[current]) {
			glDeleteSync(fences[current]);
		}

		fences[current] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		current = (current + 1) % Size;
	}

};

void Batcher::flushAll() {
	static BuffersRing<5> ring;
	ring.init(maxDrawSize);

	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");

	for (auto drawObjects : drawList) {
		auto& entityIdsBuffer = ring.getBuffer();

		SFE::GLW::VertexArray::bindArray(drawObjects->VAO);
		
		entityIdsBuffer.bind();
		entityIdsBuffer.clear();
		entityIdsBuffer.addData(drawObjects->entities);
		

		glVertexAttribIPointer(7, 1, GL_UNSIGNED_INT, 0, (void*)0); //todo initialize it somehow only once
		glEnableVertexAttribArray(7);
		glVertexAttribDivisor(7, 1);

		
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::DIFFUSE, defaultTex);
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::NORMALS, defaultNormal);
		AssetsModule::TextureHandler::bindTextureToSlot(SFE::SPECULAR, defaultTex);
	
		for (auto i = 0; i < drawObjects->materialData.materialsCount; i++) {
			const auto& mat = drawObjects->materialData.material[i];
			SFE::GLW::bindTextureToSlot(mat.slot, mat.type, mat.textureId);
		}

		SFE::GLW::drawVerticesW(drawObjects->verticesCount, drawObjects->indicesCount, drawObjects->entities.size());

		ring.rotate();
	}

	SFE::GLW::Buffer<SFE::GLW::SHADER_STORAGE_BUFFER>::bindDefaultBuffer();
	SFE::GLW::VertexArray::bindDefault();
}

void Batcher::clear() {
	for (auto& drawObj : drawList) {
		delete drawObj;
	}

	drawList.clear();
}
