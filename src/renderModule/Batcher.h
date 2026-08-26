#pragma once

#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "containersModule/Vector.h"
#include "ecss/Types.h"

struct DrawObject {
	friend bool operator==(const DrawObject& lhs, SFE::RenderMeshData meshData) {
		return lhs.meshData == meshData;
	}
	friend bool operator!=(const DrawObject& lhs, SFE::RenderMeshData meshData) {
		return !(lhs == meshData);
	}

	friend bool operator==(const DrawObject& lhs, const DrawObject& rhs) {
		return lhs.meshData == rhs.meshData;
	}
	friend bool operator!=(const DrawObject& lhs, const DrawObject& rhs) {
		return !(lhs == rhs);
	}

	SFE::RenderMeshData meshData;
	SFE::ComponentsModule::Materials materialData; //todo make it pointer too

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

	void addToDrawList(SFE::RenderMeshData VAO, const SFE::ComponentsModule::Materials& material, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms);
	void addToDrawList(SFE::RenderMeshData VAO, const SFE::ComponentsModule::Materials& material, ecss::EntityId id);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll(SFE::Render::CommandListHandle cmd, SFE::Render::DescriptorSetHandle setHandle);
	void clear();
	//map VAO - drawObj
	std::unordered_map<uint32_t, DrawObject> drawList;
	//SFE::Vector<DrawObject> drawList;
	//SFE::Vector<std::shared_ptr<DrawObject>> drawList;

	//unsigned maxDrawSize = 1000000;

	SFE::Render::BufferHandle batcherBuffer;
	static inline SFE::Math::Mat4 mBones[100]{ SFE::Math::Mat4(1.f) };
};
