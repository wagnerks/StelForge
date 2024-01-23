#pragma once

#include <vector>

#include "assetsModule/modelModule/Mesh.h"

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

	bool transparentForShadow = false;
	SFE::Math::Vec3 sortedPos = {};
	unsigned transformsBuffer;
	size_t transformsCount = 0;
	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);
};


class Batcher {
public:
	Batcher();
	void addToDrawList(unsigned VAO, size_t vertices, size_t indices, const AssetsModule::Material& material, const SFE::Math::Mat4&  transform, bool transparentForShadow);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll(bool clear = false);

	std::vector<DrawObject> drawList;

	unsigned instanceVBO = -1;
	unsigned maxDrawSize = 100000;
};
