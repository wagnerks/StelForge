#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "assetsModule/modelModule/Mesh.h"
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
	SFE::Math::Vec3 tint{ 1.f, 1.f, 1.f };

	struct Matrices {
		SFE::Math::Mat4* transform;
		SFE::Math::Mat4* bones;
	};

	std::vector<Matrices> matrices;
	std::vector<ecss::EntityId> entities;
	size_t bonesCount = 0;

	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);
};


class Batcher {
public:
	Batcher();

	void addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms);
	void addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent* material, ecss::EntityId id);
	void addToDrawList(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, ecss::EntityId id);
	void addToDrawListGlass(SFE::RenderMeshData meshData, const SFE::ComponentsModule::MaterialComponent& material, ecss::EntityId id);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline = {}, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw = {}, const std::vector<DrawObject*>* ordered = nullptr);
	void flushEntity(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& obj, ecss::EntityId entity, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline = {}, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw = {}, bool bindResources = true);
	void clear();

	std::unordered_map<DrawBatchKey, DrawObject, DrawBatchKeyHash> drawList;

	SFE::Render::BufferHandle batcherBuffer;
	static inline SFE::Math::Mat4 mBones[100]{ SFE::Math::Mat4(1.f) };

private:
	void flushEntitiesInternal(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle, DrawObject& obj, const ecss::EntityId* entityIds, size_t entityCount, const std::function<SFE::Render::PipelineHandle(const std::string&, const std::string&)>& getPipeline, const std::function<void(SFE::Render::CommandListHandle, const DrawObject&)>& beforeDraw, bool bindResources = true);
	DrawObject& batchFor(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material);
	DrawObject& batchForGlass(const SFE::RenderMeshData& meshData, const SFE::ComponentsModule::MaterialComponent& material);

	DrawBatchKey mLastKey{};
	DrawObject* mLastObj = nullptr;
	size_t mInstanceCapacity = 0;
	SFE::Render::TextureHandle mDefaultBlack{};
	SFE::Render::TextureHandle mDefaultGray{};
};
