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
	namespace {
		bool aabbOverlapsOctree(const FrustumModule::AABB& aabb, const Math::Vec3& nodePos, float nodeSize) {
			const Math::Vec3 nmin(nodePos.x, nodePos.y - nodeSize, nodePos.z);
			const Math::Vec3 nmax(nodePos.x + nodeSize, nodePos.y, nodePos.z + nodeSize);
			const Math::Vec3 amin = aabb.center - aabb.extents;
			const Math::Vec3 amax = aabb.center + aabb.extents;
			return amin.x <= nmax.x && amax.x >= nmin.x
				&& amin.y <= nmax.y && amax.y >= nmin.y
				&& amin.z <= nmax.z && amax.z >= nmin.z;
		}
	}

	OcTreeSystem::OcTreeSystem() : System({ SFE::SystemsModule::TaskType::AABB_UPDATED }) {
		curMaxX = curMaxY = curMaxZ = std::numeric_limits<float>::lowest();
		curMinX = curMinY = curMinZ = std::numeric_limits<float>::max();
	}

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
							curMaxX = (std::max)(octree.x, curMaxX);
							curMaxY = (std::max)(octree.y, curMaxY);
							curMaxZ = (std::max)(octree.z, curMaxZ);
							curMinX = (std::min)(octree.x, curMinX);
							curMinY = (std::min)(octree.y, curMinY);
							curMinZ = (std::min)(octree.z, curMinZ);
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
				if (aabbOverlapsOctree(aabb, pos, static_cast<float>(OCTREE_SIZE))) {
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
					if (aabbOverlapsOctree(aabb, pos, static_cast<float>(OCTREE_SIZE))) {
						positions.push_back(pos);
					}
				}
			}
			for (const auto& pos : positions) {
				func(pos);
			}
			return;
		}

		float maxX, minX, maxY, minY, maxZ, minZ;
		calculateAABBOctrees(aabb, OCTREE_SIZE, maxX, minX, maxY, minY, maxZ, minZ);

		if (!std::isfinite(minX) || !std::isfinite(maxX) || !std::isfinite(minY) || !std::isfinite(maxY)
			|| !std::isfinite(minZ) || !std::isfinite(maxZ) || maxX < minX || maxY < minY || maxZ < minZ) {
			return;
		}

		constexpr float kMaxAbs = 1.0e7f;
		if (std::fabs(minX) > kMaxAbs || std::fabs(maxX) > kMaxAbs
			|| std::fabs(minY) > kMaxAbs || std::fabs(maxY) > kMaxAbs
			|| std::fabs(minZ) > kMaxAbs || std::fabs(maxZ) > kMaxAbs) {
			return;
		}

		const float size = static_cast<float>(OCTREE_SIZE);
		auto axisSteps = [size](float a, float b) -> int {
			const double n = (static_cast<double>(b) - static_cast<double>(a)) / static_cast<double>(size);
			if (n > 8.0 || n < 0.0) {
				return -1;
			}
			return static_cast<int>(n) + 1;
		};
		constexpr int kMaxAxisSteps = 8;
		const int nx = axisSteps(minX, maxX);
		const int ny = axisSteps(minY, maxY);
		const int nz = axisSteps(minZ, maxZ);
		if (nx < 1 || ny < 1 || nz < 1 || nx > kMaxAxisSteps || ny > kMaxAxisSteps || nz > kMaxAxisSteps) {
			return;
		}

		for (int ix = 0; ix < nx; ++ix) {
			const float x = minX + static_cast<float>(ix) * size;
			for (int iy = 0; iy < ny; ++iy) {
				const float y = maxY - static_cast<float>(iy) * size;
				for (int iz = 0; iz < nz; ++iz) {
					func({ x, y, minZ + static_cast<float>(iz) * size });
				}
			}
		}
	}

	void OcTreeSystem::deleteOctree(const Math::Vec3& octree) {
		auto mapLock = writeLock();
		mOctrees.erase(octree);
		curMaxX = curMaxY = curMaxZ = std::numeric_limits<float>::lowest();
		curMinX = curMinY = curMinZ = std::numeric_limits<float>::max();
		for (auto& [pos, _] : mOctrees) {
			curMaxX = std::max(pos.x, curMaxX);
			curMaxY = std::max(pos.y, curMaxY);
			curMaxZ = std::max(pos.z, curMaxZ);
			curMinX = std::min(pos.x, curMinX);
			curMinY = std::min(pos.y, curMinY);
			curMinZ = std::min(pos.z, curMinZ);
		}
	}
}
