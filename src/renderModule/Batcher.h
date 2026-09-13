#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/modelModule/Material.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "containersModule/Vector.h"
#include "ecss/Types.h"
#include "renderModule/RenderHardwareInterface.h"

struct DrawBatchKey {
	uint32_t vertexBufferId = 0;
	uint64_t materialHash = 0;

	bool operator==(const DrawBatchKey& other) const {
		return vertexBufferId == other.vertexBufferId && materialHash == other.materialHash;
	}
};

struct DrawBatchKeyHash {
	size_t operator()(const DrawBatchKey& key) const {
		uint64_t h = key.vertexBufferId;
		h ^= key.materialHash + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
		return static_cast<size_t>(h);
	}
};

struct DrawObject {
	SFE::RenderMeshData meshData;
	SFE::ComponentsModule::Materials materialData;
	std::string vertexShader = SFE::ComponentsModule::DefaultGBufferVS;
	std::string fragmentShader = SFE::ComponentsModule::DefaultGBufferFS;
	float opacity = 1.f;
	float ior = 1.5f;
	float roughness = 0.5f;
	float metallic = 0.f;
	float alphaCutoff = 0.f;
	float specFactor = 1.f;
	float clearcoat = 0.f;
	float clearcoatRough = 0.04f;
	bool alphaFromMaps = false;
	bool alphaDither = false;
	SFE::Math::Vec3 tint{ 1.f, 1.f, 1.f };
	uint64_t materialHash = 0;

	struct Matrices {
		SFE::Math::Mat4* transform;
		SFE::Math::Mat4* bones;
	};

	std::vector<Matrices> matrices;
	std::vector<ecss::EntityId> entities;
	size_t bonesCount = 0;

	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);

	bool usesAlphaCutout() const {
		return fragmentShader == SFE::ComponentsModule::DefaultGBufferMaskedFS;
	}

	float cutoutCutoff() const {
		if (alphaCutoff > 0.f) {
			return alphaCutoff;
		}
		if (alphaFromMaps) {
			return 0.18f;
		}
		if (usesAlphaCutout()) {
			return 0.4f;
		}
		return 0.f;
	}

	SFE::Render::TextureHandle diffuseTexture() const {
		for (uint8_t i = 0; i < materialData.materialsCount; ++i) {
			if (materialData.material[i].slot == SFE::DIFFUSE) {
				return materialData.material[i].texture;
			}
		}
		return {};
	}

	SFE::Render::TextureHandle maskTexture() const {
		SFE::Render::TextureHandle spec{};
		SFE::Render::TextureHandle rough{};
		for (uint8_t i = 0; i < materialData.materialsCount; ++i) {
			const auto& slot = materialData.material[i];
			if (slot.slot == SFE::ROUGHNESS) {
				rough = slot.texture;
			}
			else if (slot.slot == SFE::SPECULAR) {
				spec = slot.texture;
			}
		}
		if (rough) {
			return rough;
		}
		if (spec) {
			return spec;
		}
		return diffuseTexture();
	}

	float gBufferMapMask() const {
		float mask = 0.f;
		for (uint8_t i = 0; i < materialData.materialsCount; ++i) {
			const int slot = materialData.material[i].slot;
			if (slot == SFE::ROUGHNESS) {
				mask += 1.f;
			}
			else if (slot == SFE::METALLIC) {
				mask += 2.f;
			}
			else if (slot == SFE::SPECULAR) {
				mask += 4.f;
			}
		}
		if (alphaFromMaps) {
			mask += 8.f;
		}
		return mask;
	}

	float gBufferUvSel() const {
		float sel = 0.f;
		for (uint8_t i = 0; i < materialData.materialsCount; ++i) {
			if (materialData.material[i].uvSet < 1) {
				continue;
			}
			const int slot = materialData.material[i].slot;
			if (slot == SFE::DIFFUSE) {
				sel += 1.f;
			}
			else if (slot == SFE::NORMALS) {
				sel += 2.f;
			}
			else if (slot == SFE::SPECULAR) {
				sel += 4.f;
			}
			else if (slot == SFE::ROUGHNESS) {
				sel += 8.f;
			}
			else if (slot == SFE::METALLIC) {
				sel += 16.f;
			}
			else if (slot == SFE::SUBSURFACE) {
				sel += 32.f;
			}
		}
		return sel;
	}
};


class Batcher {
public:
	Batcher();

	void addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms);
	void addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent* material, ecss::EntityId id);
	void addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, ecss::EntityId id);
	void addToDrawListGlass(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, ecss::EntityId id);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline = {}, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw = {}, const std::vector<DrawObject*>* ordered = nullptr, bool bindResources = true);
	void flushEntity(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& obj, ecss::EntityId entity, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline = {}, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw = {}, bool bindResources = true);
	void clear();
	void setMaterialLayout(SFE::Render::DescriptorSetLayoutHandle layout) { mMaterialLayout = layout; }
	SFE::Render::DescriptorSetLayoutHandle materialLayout() const { return mMaterialLayout; }

	std::unordered_map<DrawBatchKey, DrawObject, DrawBatchKeyHash> drawList;

	SFE::Render::BufferHandle batcherBuffer;
	SFE::Render::BufferHandle mBatcherBuffer[2]{};
	uint32_t mGpuFrames = 1;
	static inline SFE::Math::Mat4 mBones[SFE::ComponentsModule::kMaxArmatureBones]{ SFE::Math::Mat4(1.f) };

private:
	void flushEntitiesInternal(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& obj, const ecss::EntityId* entityIds, size_t entityCount, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, bool bindResources = true);
	DrawObject& batchFor(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material);
	DrawObject& batchForGlass(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material);

	DrawBatchKey mLastKey{};
	DrawObject* mLastObj = nullptr;
	size_t mInstanceCapacity = 0;
	// Where the next group's entity ids go this frame. cmdUpdateBuffer writes at the call, so every
	// group drawn from one list needs its own region, addressed through firstInstance.
	size_t mIdsOffset = 0;
	int mIdsFrame = -1;
	SFE::Render::TextureHandle mDefaultBlack{};
	SFE::Render::TextureHandle mDefaultGray{};
	SFE::Render::DescriptorSetLayoutHandle mMaterialLayout{};
	std::unordered_map<uint64_t, SFE::Render::DescriptorSetHandle> mCachedMaterialSets;
};
