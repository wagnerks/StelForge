#pragma once

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
		// Object layers: what never moves, and what does (characters, rigid bodies).
		static constexpr JPH::ObjectLayer kStatic = 0;
		static constexpr JPH::ObjectLayer kMoving = 1;

		PhysicsSystem();
		~PhysicsSystem() override;
		void update(float dt) override;

		// Called on the physics thread before every fixed step with its length: the place for anything that moves
		// through the world with it, such as a character controller. Register before the system starts ticking.
		void addStepCallback(std::function<void(float)> callback) { mStepCallbacks.push_back(std::move(callback)); }
		// Inside a step callback: the Engine::clockSeconds() time the step brings the world to. Steps that run back to
		// back in one update still get times a step apart, so what follows them can be eased smoothly.
		double stepTime() const { return mStepTime; }

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
		std::unique_ptr<JPH::TempAllocatorImpl> mTempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> mJobs;
		std::unique_ptr<JPH::PhysicsSystem> mWorld;
		std::vector<std::function<void(float)>> mStepCallbacks;
		float mAccumulated = 0.f;
		double mStepTime = 0.0;
	};
}
