#pragma once

#include <vector>

#include "assetsModule/modelModule/Mesh.h"

struct BonesData {
	BonesData(const std::vector<SFE::Math::Mat4>& bones) {
		for (auto i = 0; i < bones.size(); i++) {
			mBones[i] = bones[i];
		}
		for (auto i = bones.size(); i < 100; i++) {
			mBones[i] = SFE::Math::Mat4(1.f);
		}
	}

	BonesData() {
		for (auto i = 0; i < 100; i++) {
			mBones[i] = SFE::Math::Mat4(1.f);
		}
	}
	SFE::Math::Mat4 mBones[100];
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
	AssetsModule::Material material;
	std::vector<SFE::Math::Mat4> transforms;
	std::vector<BonesData> bones;

	bool transparentForShadow = false;
	SFE::Math::Vec3 sortedPos = {};
	unsigned batcherBuffer[2];
	size_t transformsCount = 0;
	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);
};


class Batcher {
public:
	Batcher();
	void addToDrawList(unsigned VAO, size_t vertices, size_t indices, const AssetsModule::Material& material, const SFE::Math::Mat4&  transform, const std::vector<SFE::Math::Mat4>& bonesTransforms, bool transparentForShadow);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll(bool clear = false);

	std::vector<DrawObject> drawList;

	unsigned instanceVBO = -1;
	unsigned maxDrawSize = 100000;
};
