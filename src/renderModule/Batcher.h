#pragma once

#include <mutex>
#include <shared_mutex>
#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "containersModule/Vector.h"
#include "containersModule/Singleton.h"
#include "ecss/Types.h"
#include "glWrapper/Buffer.h"
#include "systemsModule/SystemBase.h"

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
		ecss::EntityId entityID;
		SFE::Math::Mat4* transform;
	};

	std::vector<ecss::EntityId> entities;
	std::vector<const SFE::Math::Mat4*> transforms;

	void sortTransformAccordingToView(const SFE::Math::Vec3& viewPos);
};

class DrawDataHolder : public SFE::Singleton<DrawDataHolder> {
public:
	void init() override {
		transformsBO.generate();
		transformsBO.bind();
		transformsBO.setBufferBinding(10);

		bonesBO.generate();
		bonesBO.bind();
		bonesBO.setBufferBinding(11);
	}

	size_t getEntityIdx(ecss::EntityId entity) {
		{
			std::shared_lock shLock(mtx);
			const auto it = dataMap.find(entity);
			if (it != dataMap.end()) {
				return it->second;
			}
		}

		std::unique_lock lock(mtx);
		return dataMap.insert({ entity, entities++ }).first->second;
	}

	void updateTransform(ecss::EntityId entity, const SFE::Math::Mat4& transform, bool bind = true) {
		if (bind) {
			transformsBO.bind();
		}
		
		transformsBO.setData(1, &transform, getEntityIdx(entity));
		if (bind) {
			transformsBO.unbind();
		}
	}

	void updateBones(ecss::EntityId entity, const SFE::Math::Mat4* bones, bool bind = true) {
		if (bind) {
			bonesBO.bind();
		}
		
		bonesBO.setData(100, bones, getEntityIdx(entity) * 100);
		if (bind) {
			bonesBO.unbind();
		}
	}

	std::unordered_map<ecss::EntityId, size_t> dataMap;
	size_t entities = 0;

	SFE::GLW::ShaderStorageBuffer<SFE::Math::Mat4, SFE::GLW::DYNAMIC_DRAW> transformsBO;
	SFE::GLW::ShaderStorageBuffer<SFE::Math::Mat4, SFE::GLW::DYNAMIC_DRAW> bonesBO;

	std::shared_mutex mtx;
};


class Batcher {
public:
	Batcher() = default;

	void addToDrawList(ecss::EntityId entity, unsigned VAO, size_t vertices, size_t indices, const SFE::ComponentsModule::Materials& material, const SFE::Math::Mat4& transform);
	void sort(const SFE::Math::Vec3& viewPos = {});
	void flushAll();
	void clear();
	SFE::Vector<DrawObject*> drawList;

	std::vector<GLsync> fences;
	unsigned maxDrawSize = 10000;
	int cur = 0;
};
