#include "Batcher.h"

#include <algorithm>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/TransformComponent.h"
#include "debugModule/Benchmark.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/Draw.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	std::ranges::sort(matrices, [&viewPos](const Matrices& a, const Matrices& b) {//todo sort bones too
		return SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*a.transform)[3])) < SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*b.transform)[3]));
	});
}

Batcher::Batcher()
{
	SFE::Render::BufferDesc desc;
	desc.size = sizeof(ecss::EntityId) * 10000;
	desc.usage = SFE::Render::BufferUsage::Storage;
	desc.cpuAccess = SFE::Render::CpuAccess::Write;
	batcherBuffer = SFE::Engine::instance()->rhi()->createBuffer(desc);
}

void Batcher::addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::Materials& textures, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms) {
	if (!meshData.vertexBuffer) {
		return;
	}
	auto it = drawList.find(meshData.vertexBuffer.id);
	if (it != drawList.end()) {
		it->second.matrices.emplace_back(const_cast<SFE::Math::Mat4*>(&transform), bonesTransforms);
	}
	else {
		auto& obj  = drawList[meshData.vertexBuffer.id];
		obj.meshData = meshData;
		obj.materialData = textures;
		obj.matrices.reserve(1000000);
		obj.matrices.emplace_back(const_cast<SFE::Math::Mat4*>(&transform), bonesTransforms);
		if (bonesTransforms) {
			obj.bonesCount++;
		}
		drawList.insert({ meshData.vertexBuffer.id, obj });
	}
}

void Batcher::addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::Materials& textures, ecss::EntityId id)
{
	if (!meshData.vertexBuffer) {
		return;
	}
	auto it = drawList.find(meshData.vertexBuffer.id);
	if (it != drawList.end()) {
		it->second.entities.emplace_back(id);
	}
	else {
		auto& obj = drawList[meshData.vertexBuffer.id];
		obj.meshData = meshData;
		obj.materialData = textures;
		obj.entities.reserve(1000000);
		obj.entities.emplace_back(id);

		drawList.insert({ meshData.vertexBuffer.id, obj });
	}
}

void Batcher::sort(const SFE::Math::Vec3& viewPos) {
	/*for (auto& drawObjects : drawList) {
		drawObjects.second.sortTransformAccordingToView(viewPos);
	}*/

	/*drawList.sort([&viewPos](const std::shared_ptr<DrawObject>& a, const std::shared_ptr<DrawObject>& b) {
		return SFE::Math::distanceSqr(viewPos, (*a.get()->matrices.front().transform)[3].xyz) > SFE::Math::distanceSqr(viewPos, (*b.get()->matrices.front().transform)[3].xyz);
	});*/
}

void Batcher::flushAll() {
	SFE::Engine::instance()->rhi()->bindStorageBuffer(3, batcherBuffer);

	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");
	int i = 0;
	for (auto& drawObjects : drawList) {
		auto objs = drawObjects.second;
		SFE::Debug::BenchmarkFunc scopeObj = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__) + std::string(" flush all") + std::to_string(i++));

		SFE::Engine::instance()->rhi()->updateBuffer(batcherBuffer, 0, sizeof(ecss::EntityId) * objs.entities.size(), objs.entities.data());

		SFE::Engine::instance()->rhi()->bindTexture(SFE::DIFFUSE, defaultTex->texture, defaultTex->sampler);
		SFE::Engine::instance()->rhi()->bindTexture(SFE::NORMALS, defaultNormal->texture, defaultNormal->sampler);
		SFE::Engine::instance()->rhi()->bindTexture(SFE::SPECULAR, defaultTex->texture, defaultTex->sampler);

		for (auto i = 0; i < objs.materialData.materialsCount; i++) {
			const auto& mat = objs.materialData.material[i];
			SFE::Engine::instance()->rhi()->bindTexture(mat.slot, mat.texture);
		}

		objs.meshData.vertexBuffer.bindVertex(0, objs.meshData.vertexOffsetBytes);
		objs.meshData.indexBuffer.bindIndex(objs.meshData.indexType, objs.meshData.indexOffsetBytes);
		if (objs.meshData.indexCount) {
			SFE::Engine::instance()->rhi()->drawIndexed(objs.meshData.indexCount, 0, objs.entities.size(), objs.meshData.indexType, 0);
		}
		else {
			SFE::Engine::instance()->rhi()->draw(objs.meshData.vertexCount, 0, objs.entities.size());
		}

		SFE::SystemsModule::RenderDebugInfo::instance()->objectsRendered += objs.entities.size();
		SFE::SystemsModule::RenderDebugInfo::instance()->verticesRendered += objs.entities.size() * objs.meshData.vertexCount;
		SFE::SystemsModule::RenderDebugInfo::instance()->drawCalls++;
	}

	//todo do not forget about bones updating
	//	if (objs.bonesCount) {
	//		FUNCTION_BENCHMARK_NAMED(bones_binding)
	//		batcherBuffer.bind(1);
	//		batcherBuffer.setBufferBinding(2, 1);
	//		batcherBuffer.allocateData(sizeof(SFE::Math::Mat4) * 100, objs.matrices.size(), SFE::GLW::DYNAMIC_DRAW);
	//		auto size = sizeof(SFE::Math::Mat4) * 100;
	//		for (auto j = 0; j < objs.matrices.size(); j++) {
	//			if (objs.matrices[j].bones) {
	//				batcherBuffer.setData(size, 1, objs.matrices[j].bones, j);
	//			}
	//			else {
	//				batcherBuffer.setData(size, 1, mBones, j);
	//			}
	//		}
	//	}
}

void Batcher::clear() {
	drawList.clear();
}
