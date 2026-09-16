#include "OcTreeSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "CameraSystem.h"
#include "imgui.h"
#include "RenderSystem.h"
#include "logsModule/logger.h"
#include "systemsModule/SystemManager.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "multithreading/ThreadPool.h"
#include "ecss/Registry.h"

namespace SFE::SystemsModule {
	OcTreeSystem::OcTreeSystem() : System({ SFE::SystemsModule::TaskType::AABB_UPDATED }) {}

	void OcTreeSystem::collectVisible(const FrustumModule::Frustum& frustum, std::vector<ecss::EntityId>& out) {
		forEachObjectInFrustum(frustum, [&out](auto id) {
			out.push_back(id);
		});
	}

	ecss::Ranges<ecss::EntityId> OcTreeSystem::makeEntityRanges(std::vector<ecss::EntityId> ids) {
		if (ids.empty()) {
			return {};
		}
		std::sort(ids.begin(), ids.end());
		return ecss::Ranges<ecss::EntityId>(ids);
	}

	ecss::Ranges<ecss::EntityId> OcTreeSystem::collectVisibleRanges(const FrustumModule::Frustum& frustum) {
		std::vector<ecss::EntityId> ids;
		ids.reserve(512);
		collectVisible(frustum, ids);
		return makeEntityRanges(std::move(ids));
	}

	void OcTreeSystem::updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) {
		auto* renderSys = ECSHandler::getSystem<RenderSystem>();
		auto worldLock = renderSys ? renderSys->tryLockWorldRead() : RenderSystem::WorldReadGuard{};
		if (renderSys && !worldLock) {
			return;
		}
		ECSHandler::registry().forEachAsync<ComponentsModule::AABBComponent, OcTreeComponent>(entitiesToProcess, [this](ecss::SectorId entity, ComponentsModule::AABBComponent* aabbcomp, OcTreeComponent* component) {
			if (!aabbcomp || !component) {
				return;
			}

			auto prev = component->parents;
			for (const auto& parent : prev) {
				if (auto* tree = getOctree(parent.pos)) {
					auto lock = tree->writeLock();
					tree->erase(entity, parent.node);
				}
			}
			component->parents.clear();

			aabbcomp->mtx.lock_shared();
			for (auto aabb : aabbcomp->aabbs) {
				forEachOctreePosInAABB(aabb, [&component, &aabb, entity, this](const Math::Vec3& octree) {
					SysOcTree* tree = nullptr;
					{
						auto mapLock = writeLock();
						tree = findOctree(octree);
						if (!tree) {
							auto [it, _] = mOctrees.try_emplace(octree, octree);
							tree = &it->second;
							LogsModule::Logger::LOG_INFO("added %f %f %f octree on entity %d ", octree.x, octree.y, octree.z, entity);
						}
					}
					if (!tree) {
						return;
					}
					auto lock = tree->writeLock();
					size_t node = 0;
					if (tree->insert(aabb.center, aabb.extents, entity, &node)) {
						component->parents.push_back({ tree->mPos, node });
					}
				});
			}
			aabbcomp->mtx.unlock_shared();
		});
	}

	void OcTreeSystem::debugUpdate(float dt) {
		(void)dt;
	}

	std::vector<Math::Vec3> OcTreeSystem::getAABBOctrees(const FrustumModule::AABB& aabb) {
		std::vector<Math::Vec3> octreesInFrust;
		forEachOctreePosInAABB(aabb, [&octreesInFrust](const Math::Vec3& octreePos) {
			octreesInFrust.emplace_back(octreePos);
		}, true);
		return octreesInFrust;
	}

	void OcTreeSystem::forEachOctreeInAABB(const FrustumModule::AABB& aabb, std::function<void(SysOcTree&)> func) {
		std::vector<SysOcTree*> trees;
		{
			auto mapLock = readLock();
			if (mOctrees.empty()) {
				return;
			}
			for (auto& [pos, tree] : mOctrees) {
				if (SysOcTree::isAABBInBoxAny(aabb.center, aabb.extents, pos, static_cast<float>(OCTREE_SIZE))) {
					trees.push_back(&tree);
				}
			}
		}
		for (auto* tree : trees) {
			func(*tree);
		}
	}

	void OcTreeSystem::forEachOctreePosInAABB(const FrustumModule::AABB& aabb, std::function<void(const Math::Vec3&)> func, bool onlyExisted) {
		if (onlyExisted) {
			std::vector<Math::Vec3> positions;
			{
				auto mapLock = readLock();
				for (const auto& [pos, tree] : mOctrees) {
					if (SysOcTree::isAABBInBoxAny(aabb.center, aabb.extents, pos, static_cast<float>(OCTREE_SIZE))) {
						positions.push_back(pos);
					}
				}
			}
			for (const auto& pos : positions) {
				func(pos);
			}
			return;
		}

		forEachOctreeCell(aabb, static_cast<float>(OCTREE_SIZE), func);
	}

	void OcTreeSystem::deleteOctree(const Math::Vec3& octree) {
		auto mapLock = writeLock();
		mOctrees.erase(octree);
	}
}
