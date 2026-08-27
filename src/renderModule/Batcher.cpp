#include "Batcher.h"

#include <algorithm>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/TransformComponent.h"
#include "debugModule/Benchmark.h"
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

void Batcher::flushAll(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle) {
	auto rhi = SFE::Engine::instance()->rhi();
	auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
	auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");
	int i = 0;
	for (auto& drawObjects : drawList) {
		auto objs = drawObjects.second;
		SFE::Debug::BenchmarkFunc scopeObj = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__) + std::string(" flush all") + std::to_string(i++));

		rhi->cmdUpdateBuffer(cmd, batcherBuffer, 0, sizeof(ecss::EntityId) * objs.entities.size(), objs.entities.data());
		std::vector<SFE::Render::WriteDescriptor> writes {
		{
			{
				.set = setHandle,
				.binding = SFE::DIFFUSE,
				.type = SFE::Render::DescriptorType::CombinedImageSampler,
				.images{ {defaultTex->texture, defaultTex->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::NORMALS,
				.type = SFE::Render::DescriptorType::CombinedImageSampler,
				.images{ {defaultNormal->texture, defaultNormal->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::SPECULAR,
				.type = SFE::Render::DescriptorType::CombinedImageSampler,
				.images{ {defaultTex->texture, defaultTex->sampler} }
			},
			{
				.set = setHandle,
				.binding = 3,
				.type = SFE::Render::DescriptorType::StorageBuffer,
				.buffers{ {batcherBuffer} }
			}
		}
		};

		for (auto i = 0; i < objs.materialData.materialsCount; i++) {
			const auto& mat = objs.materialData.material[i];
			writes.push_back(SFE::Render::WriteDescriptor{
				.set = setHandle,
				.binding = static_cast<uint32_t>(mat.slot),
				.type = SFE::Render::DescriptorType::CombinedImageSampler,
				.images{ {mat.texture, defaultTex->sampler} }
				});
		}
		SFE::Engine::instance()->rhi()->updateDescriptorSets(writes);

		rhi->cmdBindDescriptorSets(cmd, {}, 0, &setHandle, 1);

		rhi->cmdBindVertexBuffer(cmd, 0, objs.meshData.vertexBuffer, objs.meshData.vertexOffsetBytes);
		rhi->cmdBindIndexBuffer(cmd, objs.meshData.indexBuffer, objs.meshData.indexType, objs.meshData.indexOffsetBytes);
		//todo bind
		if (objs.meshData.indexCount) {
			rhi->cmdDrawIndexed(cmd, objs.meshData.indexCount, objs.entities.size(), 0, 0, 0);
		}
		else {
			rhi->cmdDraw(cmd, objs.meshData.vertexCount, objs.entities.size(),0, 0);
		}

		SFE::SystemsModule::RenderDebugInfo::instance()->objectsRendered += objs.entities.size();
		SFE::SystemsModule::RenderDebugInfo::instance()->verticesRendered += objs.entities.size() * objs.meshData.vertexCount;
		SFE::SystemsModule::RenderDebugInfo::instance()->drawCalls++;
	}

	//todo do not forget about bones updating, one time allocate, then just update
	//	if (objs.bonesCount) {
	//		FUNCTION_BENCHMARK_NAMED(bones_binding)
	//		batcherBuffer.bind(1);
	//		batcherBuffer.setBufferBinding(2, 1);
	//		batcherBuffer.allocateData(sizeof(SFE::Math::Mat4) * 100, objs.matrices.size());
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
