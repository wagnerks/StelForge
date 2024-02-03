#pragma once
#include "systemsModule/SystemBase.h"
#include "core/OcTree.h"
#include "ecss/EntityHandle.h"

namespace SFE {
	namespace FrustumModule {
		struct Frustum;
		struct Plane;
	}
}

namespace SFE::SystemsModule {
	class OcTreeSystem : public ecss::System, public ThreadSynchronizer {
	public:
		inline static constexpr size_t OCTREE_SIZE = 4096;
		using SysOcTree = OcTree<ecss::EntityHandle, 5, OCTREE_SIZE>;

		OcTreeSystem();

		void update(const std::vector<ecss::SectorId>& entitiesToProcess) override;
		void debugUpdate(float dt) override;
		std::vector<Math::Vec3> getAABBOctrees(const FrustumModule::AABB& aabb);

		void forEachOctreeInAABB(const FrustumModule::AABB& aabb, std::function<void(SysOcTree&)> func);
		void forEachOctreePosInAABB(const FrustumModule::AABB& aabb, std::function<void(const Math::Vec3&)> func);

		SysOcTree* getOctree(const Math::Vec3& octree) {
			auto treeIt = mOctrees.find(octree);
			if (treeIt != mOctrees.end()) {
				return &treeIt->second;
			}

			return nullptr;
		}

		void deleteOctree(const Math::Vec3& octree);
	public:

		std::map<Math::Vec3, SysOcTree> mOctrees;

		bool drawOctrees = false;
		bool drawObjAABB = false;
		bool debugOpened = false;
	};
}
