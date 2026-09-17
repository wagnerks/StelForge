#include "GarbageSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

#include "CameraSystem.h"
#include "PhysicsSystem.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Terrain.h"
#include "componentsModule/GarbageComponent.h"
#include "componentsModule/TerrainComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/WaterComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "logsModule/logger.h"
#include "propertiesModule/PropertiesSystem.h"

namespace SFE::SystemsModule {
	namespace {
		constexpr double kInterpolationDelay = 1.25 / 60.0; // as the character's: a step, and a little margin
		// Metres past the lake's oval that the bank is looked for in.
		constexpr float kBankReach = 60.f;
		// Metres along x and z from the camera within which pieces become entities with bodies, and past which a still
		// one gives them up again.
		constexpr float kSpawnReach = 20.f;
		constexpr float kReleaseReach = 26.f;
		// Pieces made into entities in one frame at most, when the camera outruns the spawning.
		constexpr uint32_t kSpawnsPerFrame = 300;
		// Frames between an entity appearing and the buffer's copy hiding: its transform reaches the GPU a frame or two
		// after the spawn, and for that long both draw rather than neither.
		constexpr uint64_t kSwapFrames = 4;
		// Seconds between the physics thread's looks for what to wake and what to let go.
		constexpr double kWakeSeconds = 0.5;
		constexpr double kReleaseSeconds = 0.25;
		constexpr float kCellSize = 8.f;

		JPH::Quat toJolt(const Math::Quaternion<float>& q) {
			return { q.x, q.y, q.z, q.w };
		}

		Math::Quaternion<float> fromJolt(const JPH::Quat& q) {
			return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
		}

		Math::Vec3 fromJolt(const JPH::RVec3& v) {
			return { static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()) };
		}

		Math::Vec3 rotate(const Math::Quaternion<float>& q, const Math::Vec3& v) {
			const Math::Vec3 u{ q.x, q.y, q.z };
			return v + Math::cross(u, Math::cross(u, v) + v * q.w) * 2.f;
		}

		// The model's points at the size it is placed at, or none when it cannot be used.
		std::vector<Math::Vec3> scaledPoints(const ComponentsModule::GarbageItem& item, float& scale) {
			auto* model = AssetsModule::ModelLoader::instance()->load(item.model);
			auto* lods = model ? model->getLODs() : nullptr;
			if (!lods || lods->empty()) {
				return {};
			}
			std::vector<Math::Vec3> points;
			Math::Vec3 lo{ std::numeric_limits<float>::max() };
			Math::Vec3 hi{ -std::numeric_limits<float>::max() };
			for (const auto* mesh : lods->front().meshes) {
				if (!mesh) {
					continue;
				}
				for (const auto& vertex : mesh->mesh.vertices) {
					points.push_back(vertex.position);
					for (int axis = 0; axis < 3; ++axis) {
						lo[axis] = (std::min)(lo[axis], vertex.position[axis]);
						hi[axis] = (std::max)(hi[axis], vertex.position[axis]);
					}
				}
			}
			const auto extent = hi - lo;
			const float longest = (std::max)({ extent.x, extent.y, extent.z });
			if (points.size() < 4 || longest <= 0.f) {
				return {};
			}
			scale = item.size / longest;
			for (auto& p : points) {
				p = p * scale;
			}
			return points;
		}
	}

	GarbageSystem::GarbageSystem() {
		auto* physics = ECSHandler::getSystem<PhysicsSystem>();
		if (!physics) {
			return;
		}
		auto& registry = ECSHandler::registry();

		ComponentsModule::GarbageComponent garbage;
		bool found = false;
		for (const auto& [id, component] : registry.view<const ComponentsModule::GarbageComponent>()) {
			if (component) {
				garbage = *component;
				found = true;
				break;
			}
		}
		const ComponentsModule::TerrainComponent* terrain = nullptr;
		Math::Vec3 terrainPos;
		for (const auto& [id, component, transform] : registry.view<const ComponentsModule::TerrainComponent, const TransformComponent>()) {
			if (component && transform) {
				terrain = component;
				terrainPos = transform->getPos(true);
				break;
			}
		}
		for (const auto& [id, water, transform] : registry.view<const ComponentsModule::WaterComponent, const TransformComponent>()) {
			if (water && transform) {
				mHasWater = true;
				mWaterCentre = transform->getPos(true);
				mWaterHalfSize = water->size * 0.5f;
				break;
			}
		}
		if (!found || !terrain || !mHasWater || garbage.items.empty()) {
			return;
		}

		// Every kind's shape, and how it rests: upright, or on one side or the other, each with how far its lowest and
		// highest points are from its origin.
		static const std::array<Math::Quaternion<float>, 3> kRests{
			Math::Quaternion<float>{},
			Math::Quaternion<float>::fromEulerDegrees({ 90.f, 0.f, 0.f }),
			Math::Quaternion<float>::fromEulerDegrees({ 0.f, 0.f, 90.f }),
		};
		struct Reach {
			float lo = 0.f;
			float hi = 0.f;
		};
		std::vector<std::array<Reach, 3>> reaches;
		std::vector<const ComponentsModule::GarbageItem*> items;
		for (const auto& item : garbage.items) {
			float scale = 1.f;
			const auto points = scaledPoints(item, scale);
			if (points.empty()) {
				continue;
			}
			JPH::Array<JPH::Vec3> hull;
			hull.reserve(points.size());
			for (const auto& p : points) {
				hull.push_back({ p.x, p.y, p.z });
			}
			// A thin convex radius: flattened boxes and trays are only a few millimetres thick.
			JPH::ConvexHullShapeSettings settings(hull, 0.004f);
			const auto result = settings.Create();
			if (result.HasError()) {
				LogsModule::Logger::LOG_ERROR("GarbageSystem: %s: %s", item.model.c_str(), result.GetError().c_str());
				continue;
			}
			std::array<Reach, 3> reach;
			for (size_t r = 0; r < kRests.size(); ++r) {
				reach[r] = { std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
				for (const auto& p : points) {
					const float y = rotate(kRests[r], p).y;
					reach[r].lo = (std::min)(reach[r].lo, y);
					reach[r].hi = (std::max)(reach[r].hi, y);
				}
			}
			reaches.push_back(reach);
			items.push_back(&item);
			mKinds.push_back({ item.buoyancy, item.mass, scale, result.Get() });
			mKindModels.push_back(item.model);
			mKindSizes.push_back(item.size);
		}
		if (mKinds.empty()) {
			return;
		}
		std::vector<float> shoreWeights;
		std::vector<float> floatWeights;
		for (const auto* item : items) {
			shoreWeights.push_back(item->weight);
			floatWeights.push_back(item->buoyancy > 1.f ? item->weight : 0.f);
		}

		const auto& heights = AssetsModule::terrainHeights(*terrain);
		const float level = mWaterCentre.y;
		std::mt19937 random(garbage.seed);
		std::uniform_real_distribution<float> unit(0.f, 1.f);
		std::discrete_distribution<size_t> pickShore(shoreWeights.begin(), shoreWeights.end());
		const bool anyFloats = std::any_of(floatWeights.begin(), floatWeights.end(), [](float w) { return w > 0.f; });
		std::discrete_distribution<size_t> pickFloat(floatWeights.begin(), floatWeights.end());

		const float halfX = terrain->lakeWidth * 0.5f + kBankReach;
		const float halfZ = terrain->lakeLength * 0.5f + kBankReach;
		struct Placed {
			uint16_t kind = 0;
			Pose pose;
		};
		std::vector<Placed> placed;
		placed.reserve(garbage.shoreCount + garbage.floatCount);
		const auto place = [&](uint32_t count, bool onWater) {
			uint32_t done = 0;
			for (uint32_t attempt = 0; done < count && attempt < count * 400u; ++attempt) {
				const float x = mWaterCentre.x + (unit(random) * 2.f - 1.f) * halfX;
				const float z = mWaterCentre.z + (unit(random) * 2.f - 1.f) * halfZ;
				const float ground = AssetsModule::terrainSurfaceHeight(*terrain, heights, x - terrainPos.x, z - terrainPos.z) + terrainPos.y;
				const float above = ground - level;
				if (onWater ? above > -garbage.floatDepth : (above < -garbage.shoreBelow || above > garbage.shoreAbove)) {
					continue;
				}
				const auto kind = static_cast<uint16_t>(onWater ? pickFloat(random) : pickShore(random));
				// Lying one of the ways it rests, turned any way about the vertical, on the ground; or, what floats in
				// water, on the water with a part of it under.
				const size_t rest = static_cast<size_t>(unit(random) * 2.999f);
				const auto yaw = Math::Quaternion<float>::fromEulerDegrees({ 0.f, unit(random) * 360.f, 0.f });
				const auto& reach = reaches[kind][rest];
				const bool floats = mKinds[kind].buoyancy > 1.f && ground < level;
				const float y = floats ? level - reach.lo - (reach.hi - reach.lo) * 0.4f : ground - reach.lo;
				placed.push_back({ kind, { { x, y, z }, Math::normalize(yaw * kRests[rest]) } });
				++done;
			}
		};
		place(garbage.shoreCount, false);
		if (anyFloats) {
			place(garbage.floatCount, true);
		}
		if (placed.empty()) {
			return;
		}

		// In cells, so the pass culls a cell at a time and only the cells around the camera are looked through.
		Math::Vec2 lo{ std::numeric_limits<float>::max() };
		Math::Vec2 hi{ -std::numeric_limits<float>::max() };
		for (const auto& p : placed) {
			lo = { (std::min)(lo.x, p.pose.pos.x), (std::min)(lo.y, p.pose.pos.z) };
			hi = { (std::max)(hi.x, p.pose.pos.x), (std::max)(hi.y, p.pose.pos.z) };
		}
		mCellSize = kCellSize;
		mOrigin = lo;
		mCellsX = static_cast<int>((hi.x - lo.x) / kCellSize) + 1;
		mCellsZ = static_cast<int>((hi.y - lo.y) / kCellSize) + 1;
		const auto cellOf = [&](const Math::Vec3& pos) {
			const int cx = std::clamp(static_cast<int>((pos.x - mOrigin.x) / mCellSize), 0, mCellsX - 1);
			const int cz = std::clamp(static_cast<int>((pos.z - mOrigin.y) / mCellSize), 0, mCellsZ - 1);
			return static_cast<size_t>(cz) * mCellsX + cx;
		};
		std::vector<uint32_t> order(placed.size());
		std::iota(order.begin(), order.end(), 0u);
		std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return cellOf(placed[a].pose.pos) < cellOf(placed[b].pose.pos); });

		mCells.assign(static_cast<size_t>(mCellsX) * mCellsZ, {});
		mGpuPieces.resize(placed.size());
		mKindOf.resize(placed.size());
		mCellOf.resize(placed.size());
		mLast.resize(placed.size());
		for (size_t i = 0; i < order.size(); ++i) {
			const auto& p = placed[order[i]];
			mCellOf[i] = static_cast<uint32_t>(cellOf(p.pose.pos));
			auto& cell = mCells[mCellOf[i]];
			if (cell.count == 0) {
				cell.first = static_cast<uint32_t>(i);
				std::copy_n(&p.pose.pos.x, 3, cell.lo);
				std::copy_n(&p.pose.pos.x, 3, cell.hi);
			}
			++cell.count;
			for (int axis = 0; axis < 3; ++axis) {
				cell.lo[axis] = (std::min)(cell.lo[axis], p.pose.pos[axis]);
				cell.hi[axis] = (std::max)(cell.hi[axis], p.pose.pos[axis]);
			}
			auto& gpu = mGpuPieces[i];
			gpu.posScale[0] = p.pose.pos.x;
			gpu.posScale[1] = p.pose.pos.y;
			gpu.posScale[2] = p.pose.pos.z;
			gpu.posScale[3] = mKinds[p.kind].scale;
			gpu.rotation[0] = p.pose.rotation.x;
			gpu.rotation[1] = p.pose.rotation.y;
			gpu.rotation[2] = p.pose.rotation.z;
			gpu.rotation[3] = p.pose.rotation.w;
			mKindOf[i] = p.kind;
			mLast[i] = p.pose;
		}
		mCellBounds = mCells;
		mHidden.assign(mGpuPieces.size() / 32 + 1, 0u);
		mEntities.assign(mGpuPieces.size(), ecss::INVALID_ID);
		mBodies.assign(mGpuPieces.size(), JPH::BodyID{});

		physics->addStepCallback([this](float dt) { step(dt); });
	}

	// ===== Physics thread =====================================================

	void GarbageSystem::step(float dt) {
		auto* physics = ECSHandler::getSystem<PhysicsSystem>();
		auto& world = physics->world();
		auto& bodies = world.GetBodyInterfaceNoLock();
		const double before = mSimTime;
		mSimTime += dt;
		const float seconds = static_cast<float>(mSimTime);
		const Math::Vec3 focus{ mFocusX.load(std::memory_order_relaxed), mWaterCentre.y, mFocusZ.load(std::memory_order_relaxed) };
		const auto crossed = [&](double period) { return std::floor(before / period) != std::floor(mSimTime / period); };

		// Bodies for the pieces that came within reach: they lie still where they are until something moves them.
		std::vector<Handover> toMake;
		{
			std::lock_guard lock(mHandoverMutex);
			toMake.swap(mToMake);
		}
		for (const auto& make : toMake) {
			if (!mBodies[make.piece].IsInvalid()) {
				continue;
			}
			const auto& kind = mKinds[mKindOf[make.piece]];
			JPH::BodyCreationSettings settings(kind.shape, JPH::RVec3(make.pose.pos.x, make.pose.pos.y, make.pose.pos.z), toJolt(make.pose.rotation),
				JPH::EMotionType::Dynamic, PhysicsSystem::kMoving);
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = kind.mass;
			settings.mFriction = 0.7f;
			settings.mRestitution = 0.1f;
			settings.mLinearDamping = 0.2f;
			settings.mAngularDamping = 0.4f;
			settings.mUserData = make.piece + 1;
			const auto body = bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
			if (body.IsInvalid()) {
				continue; // the world is full; it is tried again when the camera comes back
			}
			mBodies[make.piece] = body;
			mLast[make.piece] = make.pose;
			mNear.push_back(make.piece);
		}

		// Now and then whatever floats and has gone still is woken, for the current to take again.
		const auto inWater = [&](const Math::Vec3& pos) {
			return std::fabs(pos.x - mWaterCentre.x) <= mWaterHalfSize && std::fabs(pos.z - mWaterCentre.z) <= mWaterHalfSize;
		};
		if (crossed(kWakeSeconds)) {
			JPH::BodyIDVector wake;
			for (const auto piece : mNear) {
				const auto id = mBodies[piece];
				if (mKinds[mKindOf[piece]].buoyancy > 0.f && !bodies.IsActive(id) && mLast[piece].pos.y < mWaterCentre.y + 0.3f
					&& inWater(mLast[piece].pos)) {
					wake.push_back(id);
				}
			}
			if (!wake.empty()) {
				bodies.ActivateBodies(wake.data(), static_cast<int>(wake.size()));
			}
		}

		// The world has finished the step before this one: its active bodies are where that step left them.
		world.GetActiveBodies(JPH::EBodyType::RigidBody, mActive);
		std::vector<Moved> moved;
		moved.reserve(mActive.size());
		const auto& locks = world.GetBodyLockInterfaceNoLock();
		const JPH::Vec3 gravity = world.GetGravity();
		for (const auto& id : mActive) {
			JPH::BodyLockWrite lock(locks, id);
			if (!lock.Succeeded()) {
				continue;
			}
			auto& body = lock.GetBody();
			const auto index = body.GetUserData();
			if (index == 0 || index > mBodies.size()) {
				continue;
			}
			const auto piece = static_cast<uint32_t>(index - 1);
			const Pose now{ fromJolt(body.GetPosition()), fromJolt(body.GetRotation()) };
			moved.push_back({ piece, mLast[piece], now });
			mLast[piece] = now;

			// Held up by the water it is in, and carried by a slow current that wanders over the lake and swells up and
			// down, so what floats bobs and drifts instead of settling.
			const float buoyancy = mKinds[mKindOf[piece]].buoyancy;
			if (buoyancy <= 0.f || !inWater(now.pos) || body.GetWorldSpaceBounds().mMin.GetY() >= mWaterCentre.y) {
				continue;
			}
			const float phase = now.pos.x * 0.37f + now.pos.z * 0.23f;
			const JPH::Vec3 current(std::sin(now.pos.z * 0.02f + seconds * 0.05f) * 0.12f, std::sin(seconds * 1.3f + phase) * 0.06f,
				std::cos(now.pos.x * 0.02f + seconds * 0.04f) * 0.12f);
			body.ApplyBuoyancyImpulse(JPH::RVec3(now.pos.x, mWaterCentre.y, now.pos.z), JPH::Vec3::sAxisY(), buoyancy, 0.6f, 0.08f, current,
				gravity, dt);
		}

		// What has come to rest out of reach gives its body up, and rests where it is.
		std::vector<Handover> released;
		if (crossed(kReleaseSeconds)) {
			for (size_t i = 0; i < mNear.size();) {
				const auto piece = mNear[i];
				const auto id = mBodies[piece];
				const auto pos = fromJolt(bodies.GetPosition(id));
				if (bodies.IsActive(id) || (std::fabs(pos.x - focus.x) <= kReleaseReach && std::fabs(pos.z - focus.z) <= kReleaseReach)) {
					++i;
					continue;
				}
				released.push_back({ piece, { pos, fromJolt(bodies.GetRotation(id)) } });
				bodies.RemoveBody(id);
				bodies.DestroyBody(id);
				mBodies[piece] = {};
				mNear[i] = mNear.back();
				mNear.pop_back();
			}
		}

		std::lock_guard lock(mHandoverMutex);
		mReleased.insert(mReleased.end(), released.begin(), released.end());
		mMoved = std::move(moved);
		mMovedAt = physics->stepTime() - dt;
		mMovedStep = dt;
		++mMovedVersion;
	}

	// ===== Main thread ========================================================

	void GarbageSystem::setHidden(uint32_t piece, bool hidden) {
		auto& word = mHidden[piece / 32];
		const uint32_t bit = 1u << (piece % 32);
		word = hidden ? (word | bit) : (word & ~bit);
	}

	void GarbageSystem::spawnNear(const Math::Vec3& focus) {
		const int x0 = std::clamp(static_cast<int>((focus.x - kSpawnReach - mOrigin.x) / mCellSize), 0, mCellsX - 1);
		const int x1 = std::clamp(static_cast<int>((focus.x + kSpawnReach - mOrigin.x) / mCellSize), -1, mCellsX - 1);
		const int z0 = std::clamp(static_cast<int>((focus.z - kSpawnReach - mOrigin.y) / mCellSize), 0, mCellsZ - 1);
		const int z1 = std::clamp(static_cast<int>((focus.z + kSpawnReach - mOrigin.y) / mCellSize), -1, mCellsZ - 1);
		std::vector<Handover> toMake;
		uint32_t budget = kSpawnsPerFrame;
		for (int cz = z0; cz <= z1 && budget > 0; ++cz) {
			for (int cx = x0; cx <= x1 && budget > 0; ++cx) {
				const auto& cell = mCells[static_cast<size_t>(cz) * mCellsX + cx];
				for (uint32_t piece = cell.first; piece < cell.first + cell.count && budget > 0; ++piece) {
					if (mEntities[piece] != ecss::INVALID_ID) {
						continue;
					}
					const auto& gpu = mGpuPieces[piece];
					if (std::fabs(gpu.posScale[0] - focus.x) > kSpawnReach || std::fabs(gpu.posScale[2] - focus.z) > kSpawnReach) {
						continue;
					}
					const Pose pose{ { gpu.posScale[0], gpu.posScale[1], gpu.posScale[2] },
						{ gpu.rotation[3], gpu.rotation[0], gpu.rotation[1], gpu.rotation[2] } };
					const auto entity = PropertiesModule::PropertiesSystem::spawnModel(mKindModels[mKindOf[piece]], pose.pos,
						Math::Vec3{ gpu.posScale[3] }, Math::degrees(pose.rotation.toEuler()));
					if (entity == ecss::INVALID_ID) {
						continue;
					}
					mEntities[piece] = entity;
					mHiding.push_back({ piece, mFrame + kSwapFrames });
					toMake.push_back({ piece, pose });
					--budget;
				}
			}
		}
		if (!toMake.empty()) {
			std::lock_guard lock(mHandoverMutex);
			mToMake.insert(mToMake.end(), toMake.begin(), toMake.end());
		}
	}

	void GarbageSystem::update(float) {
		if (mGpuPieces.empty()) {
			return;
		}
		++mFrame;
		Math::Vec3 focus;
		if (auto* cameras = ECSHandler::getSystem<CameraSystem>()) {
			if (const auto camera = ECSHandler::registry().pinComponent<const TransformComponent>(cameras->getCurrentCamera())) {
				focus = camera->getPos(true);
				mFocusX.store(focus.x, std::memory_order_relaxed);
				mFocusZ.store(focus.z, std::memory_order_relaxed);
			}
		}

		std::vector<Handover> released;
		{
			std::lock_guard lock(mHandoverMutex);
			released.swap(mReleased);
			if (mMovedVersion != mShowingVersion) {
				mShowing.swap(mMoved);
				mShowingAt = mMovedAt;
				mShowingStep = mMovedStep;
				mShowingVersion = mMovedVersion;
				mShowingSettled = false;
			}
		}
		auto& registry = ECSHandler::registry();

		{
			std::lock_guard lock(mSharedMutex);
			// Back into the buffer where they came to rest, the entity gone in the same frame.
			for (const auto& release : released) {
				if (mEntities[release.piece] != ecss::INVALID_ID) {
					registry.destroyEntity(mEntities[release.piece]);
					mEntities[release.piece] = ecss::INVALID_ID;
				}
				auto& gpu = mGpuPieces[release.piece];
				gpu.posScale[0] = release.pose.pos.x;
				gpu.posScale[1] = release.pose.pos.y;
				gpu.posScale[2] = release.pose.pos.z;
				gpu.rotation[0] = release.pose.rotation.x;
				gpu.rotation[1] = release.pose.rotation.y;
				gpu.rotation[2] = release.pose.rotation.z;
				gpu.rotation[3] = release.pose.rotation.w;
				mChanged.push_back(release.piece);
				setHidden(release.piece, false);
				// Its cell still holds it, wherever it was carried: the cell's bounds grow to take it in.
				auto& bounds = mCellBounds[mCellOf[release.piece]];
				for (int axis = 0; axis < 3; ++axis) {
					bounds.lo[axis] = (std::min)(bounds.lo[axis], release.pose.pos[axis]);
					bounds.hi[axis] = (std::max)(bounds.hi[axis], release.pose.pos[axis]);
				}
			}
			for (size_t i = 0; i < mHiding.size();) {
				if (mHiding[i].frame > mFrame) {
					++i;
					continue;
				}
				if (mEntities[mHiding[i].piece] != ecss::INVALID_ID) {
					setHidden(mHiding[i].piece, true);
				}
				mHiding[i] = mHiding.back();
				mHiding.pop_back();
			}
		}

		spawnNear(focus);

		if (mShowingSettled || mShowingStep <= 0.f) {
			return;
		}
		// Between the two poses of the last step, a little in the past, as the character is shown.
		const double shown = Engine::clockSeconds() - kInterpolationDelay;
		const float t = std::clamp(static_cast<float>((shown - (mShowingAt - mShowingStep)) / mShowingStep), 0.f, 1.f);
		for (const auto& move : mShowing) {
			if (mEntities[move.piece] == ecss::INVALID_ID) {
				continue;
			}
			auto transform = registry.pinComponent<TransformComponent>(mEntities[move.piece]);
			if (!transform) {
				continue;
			}
			transform->setPos(move.from.pos + (move.to.pos - move.from.pos) * t);
			transform->setRotate(Math::degrees(Math::slerp(move.from.rotation, move.to.rotation, t).toEuler()));
		}
		// Once it has reached the step's end it stays there until the next step: no need to write it again.
		mShowingSettled = t >= 1.f;
	}
}
