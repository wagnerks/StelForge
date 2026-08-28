#include "Batcher.h"

#include <algorithm>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/Material.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "systemsModule/systems/RenderSystem.h"
#include "renderModule/GraphicsBackend.h"
#include "core/ECSHandler.h"

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	std::ranges::sort(matrices, [&viewPos](const Matrices& a, const Matrices& b) {
		return SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*a.transform)[3])) < SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*b.transform)[3]));
	});
}

Batcher::Batcher()
{
	mInstanceCapacity = 131072;
	SFE::Render::BufferDesc desc;
	desc.size = sizeof(ecss::EntityId) * mInstanceCapacity;
	desc.usage = SFE::Render::BufferUsage::Storage;
	desc.cpuAccess = SFE::Render::CpuAccess::Write;
	batcherBuffer = SFE::Engine::instance()->rhi()->createBuffer(desc);
}

namespace {
	const SFE::ComponentsModule::MaterialComponent& emptyMaterial() {
		static const SFE::ComponentsModule::MaterialComponent kEmpty;
		return kEmpty;
	}

	static uint64_t hashMaterial(const SFE::ComponentsModule::MaterialComponent& material) {
		uint64_t h = 1469598103934665603ull;
		auto mix = [&](uint64_t v) {
			h ^= v;
			h *= 1099511628211ull;
		};
		mix(material.transparent ? 1ull : 0ull);
		mix(static_cast<uint64_t>(material.opacity * 4096.f));
		mix(static_cast<uint64_t>(material.ior * 4096.f));
		mix(static_cast<uint64_t>(material.tint.x * 4096.f));
		mix(static_cast<uint64_t>(material.tint.y * 4096.f));
		mix(static_cast<uint64_t>(material.tint.z * 4096.f));
		mix(material.materials.materialsCount);
		for (uint8_t i = 0; i < material.materials.materialsCount; ++i) {
			const auto& slot = material.materials.material[i];
			mix((static_cast<uint64_t>(slot.slot) << 32) | slot.texture.id);
		}
		if (material.vertexShader != SFE::ComponentsModule::DefaultGBufferVS) {
			mix(std::hash<std::string>{}(material.vertexShader));
		}
		if (material.fragmentShader != SFE::ComponentsModule::DefaultGBufferFS) {
			mix(std::hash<std::string>{}(material.fragmentShader));
		}
		return h;
	}

	static uint64_t hashMaterialGlass(const SFE::ComponentsModule::MaterialComponent& material) {
		uint64_t h = 1469598103934665603ull;
		auto mix = [&](uint64_t v) {
			h ^= v;
			h *= 1099511628211ull;
		};
		mix(material.transparent ? 1ull : 0ull);
		mix(static_cast<uint64_t>(material.ior * 4096.f));
		mix(material.materials.materialsCount);
		for (uint8_t i = 0; i < material.materials.materialsCount; ++i) {
			const auto& slot = material.materials.material[i];
			mix((static_cast<uint64_t>(slot.slot) << 32) | slot.texture.id);
		}
		if (material.vertexShader != SFE::ComponentsModule::DefaultGBufferVS) {
			mix(std::hash<std::string>{}(material.vertexShader));
		}
		if (material.fragmentShader != SFE::ComponentsModule::DefaultGBufferFS) {
			mix(std::hash<std::string>{}(material.fragmentShader));
		}
		return h;
	}

	DrawBatchKey makeBatchKey(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material) {
		return { meshData.vertexBuffer.id, hashMaterial(material) };
	}

	DrawBatchKey makeBatchKeyGlass(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material) {
		return { meshData.vertexBuffer.id, hashMaterialGlass(material) };
	}
}

DrawObject& Batcher::batchFor(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material) {
	const auto key = makeBatchKey(meshData, material);
	if (mLastObj && mLastKey == key) {
		return *mLastObj;
	}

	auto it = drawList.find(key);
	if (it == drawList.end()) {
		auto& obj = drawList[key];
		obj.meshData = meshData;
		obj.materialData = material.materials;
		obj.vertexShader = material.vertexShader.empty() ? SFE::ComponentsModule::DefaultGBufferVS : material.vertexShader;
		obj.fragmentShader = material.fragmentShader.empty() ? SFE::ComponentsModule::DefaultGBufferFS : material.fragmentShader;
		obj.opacity = material.opacity;
		obj.ior = material.ior;
		obj.tint = material.tint;
		obj.entities.reserve(256);
		obj.matrices.reserve(64);
		mLastKey = key;
		mLastObj = &obj;
		return obj;
	}

	mLastKey = key;
	mLastObj = &it->second;
	return it->second;
}

DrawObject& Batcher::batchForGlass(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material) {
	const auto key = makeBatchKeyGlass(meshData, material);
	if (mLastObj && mLastKey == key) {
		return *mLastObj;
	}

	auto it = drawList.find(key);
	if (it == drawList.end()) {
		auto& obj = drawList[key];
		obj.meshData = meshData;
		obj.materialData = material.materials;
		obj.vertexShader = material.vertexShader.empty() ? SFE::ComponentsModule::DefaultGBufferVS : material.vertexShader;
		obj.fragmentShader = material.fragmentShader.empty() ? SFE::ComponentsModule::DefaultGBufferFS : material.fragmentShader;
		obj.opacity = material.opacity;
		obj.ior = material.ior;
		obj.tint = material.tint;
		obj.entities.reserve(256);
		obj.matrices.reserve(64);
		mLastKey = key;
		mLastObj = &obj;
		return obj;
	}

	mLastKey = key;
	mLastObj = &it->second;
	return it->second;
}

void Batcher::addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms) {
	if (!meshData.vertexBuffer || (meshData.vertexCount == 0 && meshData.indexCount == 0)) {
		return;
	}
	auto& obj = batchFor(meshData, material);
	obj.matrices.emplace_back(const_cast<SFE::Math::Mat4*>(&transform), bonesTransforms);
	if (bonesTransforms) {
		obj.bonesCount++;
	}
}

void Batcher::addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent* material, ecss::EntityId id)
{
	if (!meshData.vertexBuffer || (meshData.vertexCount == 0 && meshData.indexCount == 0)) {
		return;
	}
	auto& obj = batchFor(meshData, material ? *material : emptyMaterial());
	obj.entities.emplace_back(id);
}

void Batcher::addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, ecss::EntityId id)
{
	addToDrawList(meshData, &material, id);
}

void Batcher::addToDrawListGlass(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, ecss::EntityId id)
{
	if (!meshData.vertexBuffer || (meshData.vertexCount == 0 && meshData.indexCount == 0)) {
		return;
	}
	auto& obj = batchForGlass(meshData, material);
	obj.entities.emplace_back(id);
}

void Batcher::sort(const SFE::Math::Vec3&) {
}

void Batcher::flushEntitiesInternal(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& objs, const ecss::EntityId* entityIds, size_t entityCount, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, bool bindResources) {
	if (!entityIds || entityCount == 0) {
		return;
	}

	auto rhi = SFE::Engine::instance()->rhi();

	if (bindResources) {
		auto defaultTex = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
		auto defaultNormal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");

		if (!mDefaultBlack) {
			SFE::Render::TextureDesc solidDesc;
			solidDesc.width = 1;
			solidDesc.height = 1;
			solidDesc.format = SFE::Render::TextureFormat::RGBA8;
			const uint8_t blackPx[4] = { 0, 0, 0, 255 };
			mDefaultBlack = rhi->createTexture(solidDesc, blackPx);
			mDefaultGray = mDefaultBlack;
		}

		std::vector<SFE::Render::WriteDescriptor> writes {
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
			.images{ {mDefaultBlack, defaultTex->sampler} }
		},
		{
			.set = setHandle,
			.binding = SFE::ROUGHNESS,
			.type = SFE::Render::DescriptorType::CombinedImageSampler,
			.images{ {mDefaultGray, defaultTex->sampler} }
		},
		{
			.set = setHandle,
			.binding = SFE::METALLIC,
			.type = SFE::Render::DescriptorType::CombinedImageSampler,
			.images{ {mDefaultBlack, defaultTex->sampler} }
		},
		};

		for (auto i = 0; i < objs.materialData.materialsCount; i++) {
			const auto& mat = objs.materialData.material[i];
			if (!mat.texture) {
				continue;
			}
			writes.push_back(SFE::Render::WriteDescriptor{
				.set = setHandle,
				.binding = static_cast<uint32_t>(mat.slot),
				.type = SFE::Render::DescriptorType::CombinedImageSampler,
				.images{ {mat.texture, defaultTex->sampler} }
				});
		}

		auto* renderSystem = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>();
		const auto frameSet = renderSystem ? renderSystem->frameSetHandle() : SFE::Render::DescriptorSetHandle{};
		if (frameSet) {
			writes.push_back(SFE::Render::WriteDescriptor{
				.set = frameSet,
				.binding = 3,
				.type = SFE::Render::DescriptorType::StorageBuffer,
				.buffers{ {batcherBuffer} }
			});
		}

		rhi->updateDescriptorSets(writes);

		SFE::Render::PipelineHandle pipeline{};
		if (getPipeline) {
			pipeline = getPipeline(objs.vertexShader, objs.fragmentShader);
			if (pipeline) {
				rhi->cmdBindPipeline(cmd, pipeline);
			}
		}
		if (beforeDraw) {
			beforeDraw(cmd, objs);
		}

		const bool vulkanGeometry = rhi->supportsGeometryPass() && !rhi->supportsFullPipeline();
		if (vulkanGeometry && pipeline && frameSet && setHandle) {
			const SFE::Render::DescriptorSetHandle sets[] = { frameSet, setHandle };
			rhi->cmdBindDescriptorSets(cmd, pipeline, 0, sets, 2);
		}
		else {
			rhi->cmdBindDescriptorSets(cmd, {}, 0, &setHandle, 1);
		}
	}

	if (entityCount > mInstanceCapacity) {
		mInstanceCapacity = entityCount + entityCount / 2 + 1024;
		SFE::Render::BufferDesc desc;
		desc.size = sizeof(ecss::EntityId) * mInstanceCapacity;
		desc.usage = SFE::Render::BufferUsage::Storage;
		desc.cpuAccess = SFE::Render::CpuAccess::Write;
		batcherBuffer = rhi->createBuffer(desc);
	}

	rhi->cmdUpdateBuffer(cmd, batcherBuffer, 0, sizeof(ecss::EntityId) * entityCount, entityIds);

	rhi->cmdBindVertexBuffer(cmd, 0, objs.meshData.vertexBuffer, objs.meshData.vertexOffsetBytes);
	rhi->cmdBindIndexBuffer(cmd, objs.meshData.indexBuffer, objs.meshData.indexType, objs.meshData.indexOffsetBytes);
	const auto instanceCount = static_cast<uint32_t>(entityCount);
	if (objs.meshData.indexCount) {
		rhi->cmdDrawIndexed(cmd, objs.meshData.indexCount, instanceCount, 0, 0, 0);
	}
	else {
		rhi->cmdDraw(cmd, objs.meshData.vertexCount, instanceCount, 0, 0);
	}

	SFE::SystemsModule::RenderDebugInfo::instance()->objectsRendered += entityCount;
	SFE::SystemsModule::RenderDebugInfo::instance()->verticesRendered += entityCount * objs.meshData.vertexCount;
	SFE::SystemsModule::RenderDebugInfo::instance()->drawCalls++;
}

void Batcher::flushEntity(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& obj, ecss::EntityId entity, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, bool bindResources) {
	flushEntitiesInternal(cmd, setHandle, obj, &entity, 1, getPipeline, beforeDraw, bindResources);
}

void Batcher::flushAll(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, const std::vector<DrawObject*>* ordered) {
	FUNCTION_BENCHMARK_NAMED(Batcher_flushAll);
	if (ordered) {
		for (DrawObject* objs : *ordered) {
			if (objs && !objs->entities.empty()) {
				flushEntitiesInternal(cmd, setHandle, *objs, objs->entities.data(), objs->entities.size(), getPipeline, beforeDraw);
			}
		}
		return;
	}
	for (auto& drawObjects : drawList) {
		auto& objs = drawObjects.second;
		if (!objs.entities.empty()) {
			flushEntitiesInternal(cmd, setHandle, objs, objs.entities.data(), objs.entities.size(), getPipeline, beforeDraw);
		}
	}
}

void Batcher::clear() {
	for (auto& [_, obj] : drawList) {
		obj.entities.clear();
		obj.matrices.clear();
		obj.bonesCount = 0;
	}
	mLastObj = nullptr;
}
