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
#include "core/ThreadPool.h"
#include "ecss/Registry.h"

namespace Engine::SystemsModule {
	OcTreeSystem::OcTreeSystem() {}

	void OcTreeSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
		ECSHandler::registry().forEach<ComponentsModule::AABBComponent, OcTreeComponent>(entitiesToProcess, [this](ecss::SectorId entity, ComponentsModule::AABBComponent* aabbcomp, OcTreeComponent* component) {
			if (!aabbcomp || !component) {
				return;
			}

			for (const auto& parentOctree : component->mParentOcTrees) {
				mOctrees[parentOctree].erase(entity);//todo think about not erasing if entity is in octree
			}
			component->mParentOcTrees.clear();

			aabbcomp->mtx.lock_shared();
			for (auto aabb : aabbcomp->aabbs) {
				forEachOctreePosInAABB(aabb, [&component, &aabb, entity, this](const Math::Vec3& octree)mutable {
					auto tree = getOctree(octree);
					if (!tree) {
						const auto it = mOctrees.insert({ octree, {octree} });
						tree = &it.first->second;
					}
					auto lock = tree->writeLock();
					if (tree->insert(aabb.center, aabb.extents, entity)) {
						component->mParentOcTrees.push_back(tree->mPos);
					}
				});
			}
			aabbcomp->mtx.unlock_shared();
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
				auto& renderData = ECSHandler::getSystem<Engine::SystemsModule::RenderSystem>()->getRenderData();
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
		});

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
		});
	}

	void OcTreeSystem::forEachOctreePosInAABB(const FrustumModule::AABB& aabb, std::function<void(const Math::Vec3&)> func) {
		float maxX, minX, maxY, minY, maxZ, minZ;//todo bug
		calculateAABBOctrees(aabb, OCTREE_SIZE, maxX, minX, maxY, minY, maxZ, minZ);

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
	}
}
