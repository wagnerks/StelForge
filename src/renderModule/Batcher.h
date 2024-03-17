#pragma once

#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "containersModule/Vector.h"

struct DrawObject {
	friend bool operator==(const DrawObject& lhs, unsigned VAO) {
		return lhs.VAO == VAO;
	}
	friend bool operator!=(const DrawObject& lhs, unsigned VAO) {
		return !(lhs == VAO);
	}

	friend bool operator==(const DrawObject& lhs, const DrawObject& rhs) {
		return lhs.VAO == rhs.VAO;
	}
	friend bool operator!=(const DrawObject& lhs, const DrawObject& rhs) {
		return !(lhs == rhs);
	}

	unsigned VAO;
	size_t verticesCount;
	size_t indicesCount;
	SFE::ComponentsModule::Materials materialData; //todo make it pointer too

	struct Matrices {
		SFE::Math::Mat4* transform;
		SFE::Math::Mat4* bones;
	};

	std::vector<Matrices> matrices;
	size_t bonesCount = 0;

	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);
};


class Batcher {
public:
	Batcher() = default;

	void addToDrawList(unsigned VAO, size_t vertices, size_t indices, const SFE::ComponentsModule::Materials& material, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll();
	void clear();
	SFE::Vector<std::shared_ptr<DrawObject>> drawList;

	unsigned maxDrawSize = 100000;

	static inline SFE::Math::Mat4 mBones[100]{ SFE::Math::Mat4(1.f) };
};
