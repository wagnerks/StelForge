#pragma once
#include "systemsModule/SystemBase.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "containersModule/OcTree.h"
#include <ecss/Ranges.h>
#include <vector>

namespace SFE::SystemsModule {
	class OcTreeSystem : public ecss::System, public ThreadSynchronizer {
	public:
		inline static constexpr size_t OCTREE_SIZE = 4096;
		using SysOcTree = OcTree<ecss::EntityId, 5, OCTREE_SIZE>;


		OcTreeSystem();

		void updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) override;
		void debugUpdate(float dt) override;
		std::vector<Math::Vec3> getAABBOctrees(const FrustumModule::AABB& aabb);
		// Takes the entities out of every octree they are in; do it before destroying them.
		void detach(const std::vector<ecss::EntityId>& entities);

		void forEachOctreeInAABB(const FrustumModule::AABB& aabb, std::function<void(SysOcTree&)> func);
		void forEachOctreePosInAABB(const FrustumModule::AABB& aabb, std::function<void(const Math::Vec3&)> func, bool onlyExisted = false);

		SysOcTree* getOctree(const Math::Vec3& octree) {
			auto mapLock = readLock();
			return findOctree(octree);
		}

		bool empty() {
			auto mapLock = readLock();
			return mOctrees.empty();
		}

		template<class Func>
		void forEachObjectInFrustum(const FrustumModule::Frustum& frustum, Func&& func) {
			std::vector<SysOcTree*> trees;
			{
				auto mapLock = readLock();
				trees.reserve(mOctrees.size());
				const float halfSize = OCTREE_SIZE * 0.5f;
				for (auto& [treePos, tree] : mOctrees) {
					// skip trees off screen before taking their locks; treePos is the left-top-near corner
					auto faces = FrustumModule::kAllFrustumFaces;
					if (FrustumModule::cullFrustumFaces(frustum, treePos + Math::Vec3{ halfSize, -halfSize, halfSize }, Math::Vec3{ halfSize }, faces)) {
						trees.push_back(&tree);
					}
				}
			}
			for (auto* tree : trees) {
				auto lock = tree->readLock();
				tree->forEachObjectInsideFrustum(frustum, func);
			}
		}

		void collectVisible(const FrustumModule::Frustum& frustum, std::vector<ecss::EntityId>& out);
		ecss::Ranges<ecss::EntityId> collectVisibleRanges(const FrustumModule::Frustum& frustum);
		static ecss::Ranges<ecss::EntityId> makeEntityRanges(std::vector<ecss::EntityId> ids);

		
		void deleteOctree(const Math::Vec3& octree);

		SysOcTree* findOctree(const Math::Vec3& octree) {
			auto treeIt = mOctrees.find(octree);
			if (treeIt != mOctrees.end()) {
				return &treeIt->second;
			}
			return nullptr;
		}

	public:

		std::map<Math::Vec3, SysOcTree> mOctrees;

		bool drawOctrees = false;
		bool drawObjAABB = false;
		bool debugOpened = false;
	};
}
