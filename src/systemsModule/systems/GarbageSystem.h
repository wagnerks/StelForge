#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	// The scene's GarbageComponent as litter strewn along the lake's bank and floating on it: tens of thousands of
	// pieces, each lying where it was placed or last came to rest. GarbagePass draws them all from one buffer. Only
	// those around the camera are more: a model entity and a loose Jolt body the character can push, kick, carry and
	// throw, stepping with the world on the physics thread, where the water holds up what is in it and a slow current
	// keeps it drifting. Once one is out of reach again and still, its body and entity go and it is drawn from the
	// buffer again, where it came to rest.
	class GarbageSystem : public ecss::System {
	public:
		// Mirrors SfeGarbagePiece in garbage.vs. std430.
		struct GpuPiece {
			float posScale[4]{}; // xyz where its origin is, w its scale
			float rotation[4]{}; // quaternion xyzw
		};
		// A square of the placement grid: its pieces are one range of the piece array, and all of them are inside
		// the bounds.
		struct Cell {
			uint32_t first = 0;
			uint32_t count = 0;
			float lo[3]{};
			float hi[3]{};
		};

		GarbageSystem();
		// Main thread.
		void update(float dt) override;

		// What GarbagePass reads; fixed once the system is built.
		const std::vector<std::string>& kindModels() const { return mKindModels; }
		const std::vector<uint16_t>& kindOf() const { return mKindOf; }
		const std::vector<float>& kindSizes() const { return mKindSizes; } // metres along the longest side
		const std::vector<Cell>& cells() const { return mCells; }
		// Under the lock: the pieces in cell order; one bit per piece set while its entity draws it instead; the
		// pieces whose record changed since the last call, which it clears; and each cell's bounds.
		template<class Read>
		void readPieces(Read&& read) {
			std::lock_guard lock(mSharedMutex);
			read(mGpuPieces, mHidden, mChanged, mCellBounds);
		}

	private:
		struct Pose {
			Math::Vec3 pos;
			Math::Quaternion<float> rotation;
		};
		struct Kind {
			float buoyancy = 0.f;
			float mass = 0.f;
			float scale = 1.f;
			JPH::ShapeRefC shape;
		};
		// A piece that moved over one step, from where to where.
		struct Moved {
			uint32_t piece = 0;
			Pose from;
			Pose to;
		};
		// Handed between the threads: a body to make for a piece that came within reach, or a piece whose body has
		// gone, where it rests.
		struct Handover {
			uint32_t piece = 0;
			Pose pose;
		};
		struct Hiding {
			uint32_t piece = 0;
			uint64_t frame = 0;
		};

		// Physics thread.
		void step(float dt);

		// Main thread.
		void spawnNear(const Math::Vec3& focus);
		void setHidden(uint32_t piece, bool hidden);

		std::vector<Kind> mKinds;
		std::vector<std::string> mKindModels;
		std::vector<float> mKindSizes;
		std::vector<uint16_t> mKindOf;

		float mCellSize = 0.f;
		int mCellsX = 0;
		int mCellsZ = 0;
		Math::Vec2 mOrigin;
		std::vector<Cell> mCells;
		std::vector<uint32_t> mCellOf; // per piece

		bool mHasWater = false;
		Math::Vec3 mWaterCentre;
		float mWaterHalfSize = 0.f;

		// Shared with GarbagePass.
		std::mutex mSharedMutex;
		std::vector<GpuPiece> mGpuPieces;
		std::vector<uint32_t> mHidden;
		std::vector<uint32_t> mChanged;
		std::vector<Cell> mCellBounds;

		// Main thread.
		std::vector<ecss::EntityId> mEntities; // per piece, INVALID_ID while it has none
		std::vector<Hiding> mHiding;
		uint64_t mFrame = 0;
		std::vector<Moved> mShowing;
		double mShowingAt = 0.0;
		float mShowingStep = 0.f;
		uint64_t mShowingVersion = 0;
		bool mShowingSettled = true;

		// Physics thread.
		std::vector<JPH::BodyID> mBodies; // per piece, invalid while it has none
		std::vector<uint32_t> mNear;      // the pieces that have one
		std::vector<Pose> mLast;          // per piece with a body: where it was when last seen moving
		double mSimTime = 0.0;
		JPH::BodyIDVector mActive;

		// Written by the main thread, read by the physics thread: where the camera is.
		std::atomic<float> mFocusX{ 0.f };
		std::atomic<float> mFocusZ{ 0.f };

		// Handed between the threads.
		std::mutex mHandoverMutex;
		std::vector<Handover> mToMake;
		std::vector<Handover> mReleased;
		std::vector<Moved> mMoved;
		double mMovedAt = 0.0;
		float mMovedStep = 0.f;
		uint64_t mMovedVersion = 0;
	};
}
