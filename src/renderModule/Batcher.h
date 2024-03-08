#pragma once

#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "containersModule/Vector.h"

struct BonesData {
	BonesData() = default;
	BonesData(const std::vector<SFE::Math::Mat4>& bones) {
		std::copy(bones.begin(), bones.end(), mBones);
	}

	BonesData(SFE::Math::Mat4* bones) {
		if (!bones) {
			return;
		}
		
		std::memcpy(mBones, bones, sizeof(mBones));//maybe mat4 is not trivially copyable.. todo
	}

	SFE::Math::Mat4 mBones[100]{ SFE::Math::Mat4(1.f) };
};

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
	SFE::ComponentsModule::Materials materialData;

	std::vector<SFE::Math::Mat4> transforms;
	std::vector<BonesData> bones;

	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);
};


class Batcher {
public:
	Batcher() = default;

	void addToDrawList(unsigned VAO, size_t vertices, size_t indices, const SFE::ComponentsModule::Materials& material, const SFE::Math::Mat4& transform, SFE::Math::Mat4* bonesTransforms);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll(bool clear = false);

	SFE::Vector<DrawObject> drawList;

	unsigned maxDrawSize = 100000;
};
