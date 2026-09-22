#pragma once

#include <atomic>

#include <functional>
#include <memory>
#include <vector>
#include <optional>

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "mathModule/Forward.h"
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	// The Jolt world. Static colliders are built from the scene when the system is created (every entity with a
	// TerrainComponent, from its mesh); moving things step at a fixed rate from update(), which runs on the physics
	// tick thread (or stepped from the frame, for a fixed-step run or a capture). Nothing here touches the ECS after
	// construction.
	class PhysicsSystem : public ecss::System {
	public:
		// Object layers: what never moves, and what does (characters, rigid bodies); and what touches nothing while it is
		// moved by hand (a thing carried), which still moves and is still found by what asks for kHeld.
		static constexpr JPH::ObjectLayer kStatic = 0;
		static constexpr JPH::ObjectLayer kMoving = 1;
		static constexpr JPH::ObjectLayer kHeld = 2;

		PhysicsSystem();
		~PhysicsSystem() override;
		void update(float dt) override;

		/// @brief An upright capsule standing in the world: where its foot is, how thick and how tall.
		struct StaticCapsule {
			Math::Vec3 base;
			float radius = 0.f;
			float height = 0.f;
		};
		// Static bodies a game brings -- the trunks of its trees, its boulders. Called once, by addStaticCapsuleBodies;
		// fill the list with everything that stands. Register while the game's systems are created.
		void addStaticCapsules(std::function<void(std::vector<StaticCapsule>&)> source) { mCapsuleSources.push_back(std::move(source)); }
		// The registered capsules into the world. After the game's systems exist -- they register them -- and before
		// the world starts stepping; the terrain is in from the start, for whatever those systems stand on it.
		void addStaticCapsuleBodies();
		// The capsules another world was given, for this one too (LoosePhysicsSystem). Before addStaticCapsuleBodies.
		void shareStaticCapsules(const PhysicsSystem& with) { mCapsuleSources = with.mCapsuleSources; }

		// Called on the physics thread before every fixed step with its length: the place for anything that moves
		// through the world with it, such as a character controller. Register before the system starts ticking.
		void addStepCallback(std::function<void(float)> callback) { mStepCallbacks.push_back(std::move(callback)); }
		// Inside a step callback: the Engine::clockSeconds() time the step brings the world to. Steps that run back to
		// back in one update still get times a step apart, so what follows them can be eased smoothly.
		double stepTime() const { return mStepTime; }
		// What a step costs, eased over the last second or so: milliseconds for the step callbacks and the world's
		// update, and the bodies in the world, awake and all.
		struct Stats {
			float stepMs = 0.f;
			uint32_t bodies = 0;
			uint32_t activeBodies = 0;
		};
		Stats stats() const { return { mStepMs.load(std::memory_order_relaxed), mBodyCount.load(std::memory_order_relaxed), mActiveCount.load(std::memory_order_relaxed) }; }

		JPH::PhysicsSystem& world() { return *mWorld; }
		JPH::TempAllocator& tempAllocator() { return *mTempAllocator; }
		const JPH::ObjectVsBroadPhaseLayerFilter& broadPhaseFilter() const;
		const JPH::ObjectLayerPairFilter& layerPairFilter() const;

		// Where a ray from `from` along `direction` (not normalised: its length is the reach) first hits something,
		// as a fraction of that reach. Physics thread, from a step callback: not while the world is stepping.
		std::optional<float> castRay(const Math::Vec3& from, const Math::Vec3& direction) const;

	private:
		void addStaticColliders();

		struct Layers;
		std::unique_ptr<Layers> mLayers;
		std::unique_ptr<JPH::TempAllocator> mTempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> mJobs;
		std::unique_ptr<JPH::PhysicsSystem> mWorld;
		std::vector<std::function<void(float)>> mStepCallbacks;
		std::vector<std::function<void(std::vector<StaticCapsule>&)>> mCapsuleSources;
		float mAccumulated = 0.f;
		std::atomic<float> mStepMs{ 0.f };
		std::atomic<uint32_t> mBodyCount{ 0 };
		std::atomic<uint32_t> mActiveCount{ 0 };
		double mStepTime = 0.0;
	};

	// A second Jolt world, stepped on a thread of its own at the same rate, standing on the same static colliders: for
	// the many loose bodies a game strews about (litter, debris). However many of them tumble at once and however long
	// its steps take, the main world -- the characters -- steps on undisturbed. The two worlds do not touch: what moves
	// in both (a character) is a body in each, a kinematic one here that pushes the loose bodies it walks into.
	class LoosePhysicsSystem final : public PhysicsSystem {
	};
}
