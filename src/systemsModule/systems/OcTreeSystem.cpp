#include "OcTreeSystem.h"

#include <random>

#include "CameraSystem.h"
#include "imgui.h"
#include "RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "multithreading/ThreadPool.h"
#include "ecss/Registry.h"

namespace SFE::SystemsModule {
	OcTreeSystem::OcTreeSystem() {
		curMaxX = curMaxY = curMaxZ = std::numeric_limits<float>::min();
		curMinX = curMinY = curMinZ = std::numeric_limits<float>::max();
	}

	void OcTreeSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
		ECSHandler::registry().forEachAsync<ComponentsModule::AABBComponent, OcTreeComponent>(entitiesToProcess, [this](ecss::SectorId entity, ComponentsModule::AABBComponent* aabbcomp, OcTreeComponent* component) {
			if (!aabbcomp || !component) {
				return;
			}

			//for (const auto& [parentOctree, subtrees] : component->mParentSubOcTrees) {//todo if object erased but didn't add to octree yet, it will not be rendered..., i need to add obj to octree first, than erase, but how?
			//	auto lock = mOctrees[parentOctree].writeLock();
			//	mOctrees[parentOctree].erase(entity);//todo think about not erasing if entity is in octree
			//}
			//component->mParentSubOcTrees.clear();
			auto prev = component->mParentOcTrees;
			component->mParentOcTrees.clear();

			aabbcomp->mtx.lock_shared();
			for (auto aabb : aabbcomp->aabbs) {
				forEachOctreePosInAABB(aabb, [&component, &aabb, entity, this, &prev](const Math::Vec3& octree)mutable {
					auto tree = getOctree(octree);
					if (!tree) {

						curMaxX = std::max(octree.x, curMaxX);
						curMaxY = std::max(octree.y, curMaxY);
						curMaxZ = std::max(octree.z, curMaxZ);

						curMinX = std::min(octree.x, curMinX);
						curMinY = std::min(octree.y, curMinY);
						curMinZ = std::min(octree.z, curMinZ);

						const auto it = mOctrees.insert({ octree, {octree} });
						tree = &it.first->second;
					}
					auto lock = tree->writeLock();

					
					if (auto it = std::find(prev.begin(), prev.end(), tree->mPos); it != prev.end()) {
						mOctrees[tree->mPos].erase(entity);
						prev.erase(it);//todo bug if entity have many aabbs it can multiple times be deleted
					}
					std::vector<Math::Vec3> subOctrees;
					if (tree->insert(aabb.center, aabb.extents, entity)) {
						if (std::find(component->mParentOcTrees.begin(), component->mParentOcTrees.end(), tree->mPos) == component->mParentOcTrees.end()) {
							component->mParentOcTrees.push_back(tree->mPos);
						}
					}
				});
			}
			aabbcomp->mtx.unlock_shared();

			for (const auto& parentOctree : prev) {
				auto lock = mOctrees[parentOctree].writeLock();
				mOctrees[parentOctree].erase(entity);
			}
		});
	}

	void OcTreeSystem::debugUpdate(float dt) {
		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Debug")) {
					if (ImGui::BeginMenu("Systems debug")) {
						ImGui::Checkbox("OcTreeSystem debug", &debugOpened);
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
			}
			ImGui::EndMainMenuBar();
		}
		if (debugOpened) {
			if (ImGui::Begin("OcTreeSystemDebug", &debugOpened)) {
				ImGui::Text("octree size: %f\noctrees count: %d", OCTREE_SIZE, mOctrees.size());

				ImGui::Checkbox("drawOctrees", &drawOctrees);
				ImGui::Checkbox("drawObjAABB", &drawObjAABB);

				ImGui::Text("entities: %d", ECSHandler::registry().getComponentContainer<TransformComponent>()->size());
			}
			ImGui::End();

			if (drawOctrees) {
				auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
				forEachOctreePosInAABB(renderData.mNextCamFrustum.generateAABB(), [this](const Math::Vec3& pos) {
					if (auto tree = getOctree(pos)) {
						tree->drawOctree(drawObjAABB);
					}
					else {
						//mOctrees[octreePos] = { octreePos };
					}
				});
			}
		}
		
	}

	std::vector<Math::Vec3> OcTreeSystem::getAABBOctrees(const FrustumModule::AABB& aabb) {
		std::vector<Math::Vec3> octreesInFrust;
		forEachOctreePosInAABB(aabb, [&octreesInFrust](const Math::Vec3& octreePos)mutable {
			octreesInFrust.emplace_back(octreePos);
		}, true);

		return octreesInFrust;
	}

	void OcTreeSystem::forEachOctreeInAABB(const FrustumModule::AABB& aabb, std::function<void(SysOcTree&)> func) {
		if (mOctrees.empty()) {
			return;
		}

		forEachOctreePosInAABB(aabb, [this, func](const Math::Vec3& pos) {
			if (auto tree = getOctree(pos)) {
				func(*tree);
			}
		}, true);
	}

	void OcTreeSystem::forEachOctreePosInAABB(const FrustumModule::AABB& aabb, std::function<void(const Math::Vec3&)> func, bool onlyExisted) {
		float maxX, minX, maxY, minY, maxZ, minZ;//todo bug
		calculateAABBOctrees(aabb, OCTREE_SIZE, maxX, minX, maxY, minY, maxZ, minZ);
		if (onlyExisted) {
			maxX = std::min(curMaxX, maxX);
			maxY = std::min(curMaxY, maxY);
			maxZ = std::min(curMaxZ, maxZ);

			minX = std::max(curMinX, minX);
			minY = std::max(curMinY, minY);
			minZ = std::max(curMinZ, minZ);
		}

		for (auto i = minX;  i < maxX; i += OCTREE_SIZE) {
			for (auto j = maxY; minY < j; j -= OCTREE_SIZE) {
				for (auto k = minZ; k < maxZ; k += OCTREE_SIZE) {
					func({ i,j,k });
				}
			}
		}
	}

	void OcTreeSystem::deleteOctree(const Math::Vec3& octree) {
		mOctrees.erase(octree);
		for (auto& [pos, _] : mOctrees) {//todo calculate this stuff while adding new octree to map
			curMaxX = std::max(pos.x, curMaxX);
			curMaxY = std::max(pos.y, curMaxY);
			curMaxZ = std::max(pos.z, curMaxZ);

			curMinX = std::min(pos.x, curMinX);
			curMinY = std::min(pos.y, curMinY);
			curMinZ = std::min(pos.z, curMinZ);
		}
	}
}
