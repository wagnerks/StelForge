#include "Batcher.h"

#include <algorithm>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/Material.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "systemsModule/systems/RenderSystem.h"
#include "core/ECSHandler.h"

void DrawObject::sortTransformAccordingToView(const SFE::Math::Vec3& viewPos) {
	std::ranges::sort(matrices, [&viewPos](const Matrices& a, const Matrices& b) {
		return SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*a.transform)[3])) < SFE::Math::distanceSqr(viewPos, SFE::Math::Vec3((*b.transform)[3]));
	});
}

Batcher::Batcher()
{
	mInstanceCapacity = 131072;
	auto* rhi = SFE::Engine::instance()->rhi();
	mGpuFrames = rhi ? (std::max)(1u, (std::min)(2u, rhi->framesInFlight())) : 1u;
	SFE::Render::BufferDesc desc;
	desc.size = sizeof(ecss::EntityId) * mInstanceCapacity;
	desc.usage = SFE::Render::BufferUsage::Storage;
	desc.cpuAccess = SFE::Render::CpuAccess::Write;
	for (uint32_t i = 0; i < mGpuFrames; ++i) {
		mBatcherBuffer[i] = rhi->createBuffer(desc);
	}
	batcherBuffer = mBatcherBuffer[0];
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
		mix(material.alphaFromMaps ? 1ull : 0ull);
		mix(material.alphaDither ? 1ull : 0ull);
		mix(static_cast<uint64_t>(material.opacity * 4096.f));
		mix(static_cast<uint64_t>(material.ior * 4096.f));
		mix(static_cast<uint64_t>(material.roughness * 4096.f));
		mix(static_cast<uint64_t>(material.metallic * 4096.f));
		mix(static_cast<uint64_t>(material.alphaCutoff * 4096.f));
		mix(static_cast<uint64_t>(material.specFactor * 4096.f));
		mix(static_cast<uint64_t>(material.clearcoat * 4096.f));
		mix(static_cast<uint64_t>(material.clearcoatRough * 4096.f));
		mix(static_cast<uint64_t>(material.tint.x * 4096.f));
		mix(static_cast<uint64_t>(material.tint.y * 4096.f));
		mix(static_cast<uint64_t>(material.tint.z * 4096.f));
		mix(material.materials.materialsCount);
		for (uint8_t i = 0; i < material.materials.materialsCount; ++i) {
			const auto& slot = material.materials.material[i];
			mix((static_cast<uint64_t>(slot.slot) << 32) | slot.texture.id);
			mix(static_cast<uint64_t>(slot.uvSet));
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
			mix(static_cast<uint64_t>(slot.uvSet));
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
		obj.roughness = material.roughness;
		obj.metallic = material.metallic;
		obj.alphaCutoff = material.alphaCutoff;
		obj.specFactor = material.specFactor;
		obj.clearcoat = material.clearcoat;
		obj.clearcoatRough = material.clearcoatRough;
		obj.alphaFromMaps = material.alphaFromMaps;
		obj.alphaDither = material.alphaDither;
		obj.tint = material.tint;
		obj.materialHash = key.materialHash;
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
		obj.roughness = material.roughness;
		obj.metallic = material.metallic;
		obj.specFactor = material.specFactor;
		obj.clearcoat = material.clearcoat;
		obj.clearcoatRough = material.clearcoatRough;
		obj.tint = material.tint;
		obj.materialHash = key.materialHash;
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
	const uint32_t slot = rhi->frameInFlight() % mGpuFrames;
	batcherBuffer = mBatcherBuffer[slot];

	auto* renderSystem = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>();
	const int frame = renderSystem ? renderSystem->renderedFrames() : 0;
	if (frame != mIdsFrame) {
		mIdsFrame = frame;
		mIdsOffset = 0;
	}
	const size_t idsOffset = mIdsOffset;
	mIdsOffset += entityCount;

	// Growing replaces the buffer; draws already recorded this frame keep the one they bound.
	if (mIdsOffset > mInstanceCapacity) {
		mInstanceCapacity = mIdsOffset + mIdsOffset / 2 + 1024;
		SFE::Render::BufferDesc desc;
		desc.size = sizeof(ecss::EntityId) * mInstanceCapacity;
		desc.usage = SFE::Render::BufferUsage::Storage;
		desc.cpuAccess = SFE::Render::CpuAccess::Write;
		for (uint32_t i = 0; i < mGpuFrames; ++i) {
			mBatcherBuffer[i] = rhi->createBuffer(desc);
		}
		batcherBuffer = mBatcherBuffer[slot];
	}

	const auto frameSet = renderSystem ? renderSystem->frameSetHandle() : SFE::Render::DescriptorSetHandle{};

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
			const uint8_t grayPx[4] = { 128, 128, 128, 255 };
			mDefaultGray = rhi->createTexture(solidDesc, grayPx);
		}

		SFE::Render::DescriptorSetHandle materialSet = setHandle;
		if (mMaterialLayout) {
			auto& cached = mCachedMaterialSets[objs.materialHash];
			if (!cached) {
				cached = rhi->allocateDescriptorSet(mMaterialLayout);
				std::vector<SFE::Render::WriteDescriptor> writes {
				{
					.set = cached,
					.binding = SFE::DIFFUSE,
					.images{ {defaultTex->texture, defaultTex->sampler} }
				},
				{
					.set = cached,
					.binding = SFE::NORMALS,
					.images{ {defaultNormal->texture, defaultNormal->sampler} }
				},
				{
					.set = cached,
					.binding = SFE::SPECULAR,
					.images{ {mDefaultBlack, defaultTex->sampler} }
				},
				{
					.set = cached,
					.binding = SFE::ROUGHNESS,
					.images{ {mDefaultGray, defaultTex->sampler} }
				},
				{
					.set = cached,
					.binding = SFE::METALLIC,
					.images{ {mDefaultBlack, defaultTex->sampler} }
				},
				{
					.set = cached,
					.binding = SFE::SUBSURFACE,
					.images{ {mDefaultBlack, defaultTex->sampler} }
				},
				};
				for (auto i = 0; i < objs.materialData.materialsCount; i++) {
					const auto& mat = objs.materialData.material[i];
					if (!mat.texture) {
						continue;
					}
					writes.push_back(SFE::Render::WriteDescriptor{
						.set = cached,
						.binding = static_cast<uint32_t>(mat.slot),
						.images{ {mat.texture, defaultTex->sampler} }
						});
				}
				rhi->updateDescriptorSets(writes);
			}
			materialSet = cached;
		}
		else if (setHandle) {
			std::vector<SFE::Render::WriteDescriptor> writes {
			{
				.set = setHandle,
				.binding = SFE::DIFFUSE,
				.images{ {defaultTex->texture, defaultTex->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::NORMALS,
				.images{ {defaultNormal->texture, defaultNormal->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::SPECULAR,
				.images{ {mDefaultBlack, defaultTex->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::ROUGHNESS,
				.images{ {mDefaultGray, defaultTex->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::METALLIC,
				.images{ {mDefaultBlack, defaultTex->sampler} }
			},
			{
				.set = setHandle,
				.binding = SFE::SUBSURFACE,
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
					.images{ {mat.texture, defaultTex->sampler} }
					});
			}
			rhi->updateDescriptorSets(writes);
		}

		if (frameSet && batcherBuffer) {
			rhi->updateDescriptorSets({ {
				.set = frameSet,
				.binding = 3,
				.buffers{ {batcherBuffer} }
			} });
		}
		setHandle = materialSet;
	}
	// Written every time. Several batchers share one set, so a per-batcher record of "what I last wrote"
	// went stale as soon as another batcher wrote the same binding. It only held on OpenGL, where one
	// frame slot keeps the buffer handle constant; Vulkan alternates slots and so always rewrote it.
	else if (setHandle && batcherBuffer) {
		rhi->updateDescriptorSets({
			{
				.set = setHandle,
				.binding = 3,
				.buffers{ {batcherBuffer} }
			}
		});
	}

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

	if (bindResources && pipeline && frameSet && setHandle) {
		const SFE::Render::DescriptorSetHandle sets[] = { frameSet, setHandle };
		rhi->cmdBindDescriptorSets(cmd, pipeline, 0, sets, 2);
	}
	else if (setHandle) {
		rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &setHandle, 1);
	}

	rhi->cmdUpdateBuffer(cmd, batcherBuffer, sizeof(ecss::EntityId) * idsOffset, sizeof(ecss::EntityId) * entityCount, entityIds);

	rhi->cmdBindVertexBuffer(cmd, 0, objs.meshData.vertexBuffer, objs.meshData.vertexOffsetBytes);
	rhi->cmdBindIndexBuffer(cmd, objs.meshData.indexBuffer, objs.meshData.indexType, objs.meshData.indexOffsetBytes);
	const auto instanceCount = static_cast<uint32_t>(entityCount);
	if (objs.meshData.indexCount) {
		rhi->cmdDrawIndexed(cmd, objs.meshData.indexCount, instanceCount, 0, 0, static_cast<uint32_t>(idsOffset));
	}
	else {
		rhi->cmdDraw(cmd, objs.meshData.vertexCount, instanceCount, 0, static_cast<uint32_t>(idsOffset));
	}

	SFE::SystemsModule::RenderDebugInfo::instance()->objectsRendered += entityCount;
	SFE::SystemsModule::RenderDebugInfo::instance()->verticesRendered += entityCount * objs.meshData.vertexCount;
	SFE::SystemsModule::RenderDebugInfo::instance()->drawCalls++;
}

void Batcher::flushEntity(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& obj, ecss::EntityId entity, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, bool bindResources) {
	flushEntitiesInternal(cmd, setHandle, obj, &entity, 1, getPipeline, beforeDraw, bindResources);
}

void Batcher::flushAll(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, const std::vector<DrawObject*>* ordered, bool bindResources) {
	FUNCTION_BENCHMARK_NAMED(Batcher_flushAll);
	if (ordered) {
		for (DrawObject* objs : *ordered) {
			if (objs && !objs->entities.empty()) {
				flushEntitiesInternal(cmd, setHandle, *objs, objs->entities.data(), objs->entities.size(), getPipeline, beforeDraw, bindResources);
			}
		}
		return;
	}
	for (auto& drawObjects : drawList) {
		auto& objs = drawObjects.second;
		if (!objs.entities.empty()) {
			flushEntitiesInternal(cmd, setHandle, objs, objs.entities.data(), objs.entities.size(), getPipeline, beforeDraw, bindResources);
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
