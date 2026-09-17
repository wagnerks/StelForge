#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "core/InputHandler.h"
#include "mathModule/Forward.h"
#include "systemsModule/SystemBase.h"

namespace JPH {
	class CharacterVirtual;
}
namespace AssetsModule {
	class Animation;
}

namespace SFE::SystemsModule {
	// Runs the scene's CharacterComponent entity as the player. The body is a Jolt virtual character stepped on the
	// physics thread with the world; this system, on the main thread, only takes what that step produced: the pose,
	// eased between physics ticks, the animation to play, and a camera orbiting behind.
	//
	// WASD moves relative to the camera, Shift sprints, Ctrl walks, Space jumps, the mouse turns the camera and the
	// wheel brings it closer. Insert switches between the character and the free camera.
	//
	// Loose bodies in the world: walked into, they are pushed; E picks up the nearest one in front and puts it down
	// again, the left mouse button throws what is carried where the camera looks, F kicks what is at the feet.
	class CharacterSystem : public ecss::System, CoreModule::InputObserver {
	public:
		CharacterSystem();
		~CharacterSystem() override;
		// Main thread.
		void update(float dt) override;

	private:
		enum class State : uint8_t { Idle, Walk, Run, Sprint, JumpStart, Airborne, Land, Count };

		// What one physics step leaves for the main thread.
		struct Pose {
			Math::Vec3 feet;
			float yaw = 0.f;
			double at = 0.0; // PhysicsSystem::stepTime() of the step that made it
		};

		// Physics thread.
		void step(float dt);
		State nextState(bool onGround, float speed, float fall) const;
		// Picking up, carrying, throwing and kicking loose bodies, before the character moves.
		void handleBodies(float dt, const Math::Vec3& feet, const Math::Vec3& look);
		// The loose (dynamic) bodies whose bounds reach into a box about `centre`.
		std::vector<JPH::BodyID> bodiesNear(const Math::Vec3& centre, const Math::Vec3& halfExtent) const;

		// Main thread.
		void setActive(bool active);
		// V: the camera at the eyes, the character hidden; or back behind it.
		void setFirstPerson(bool firstPerson);
		void follow(const Math::Vec3& feet);

		bool keyDown(CoreModule::InputKey key) const;
		float waterDepthAt(const Math::Vec3& point) const;

		ecss::EntityId mEntity = ecss::INVALID_ID;
		std::unique_ptr<JPH::CharacterVirtual> mBody;

		// Copied from the scene at construction, so the physics thread never reads the ECS.
		float mWalkSpeed = 0.f;
		float mRunSpeed = 0.f;
		float mSprintSpeed = 0.f;
		float mJumpHeight = 0.f;
		bool mHasWater = false;
		Math::Vec3 mWaterCentre;
		float mWaterHalfSize = 0.f;
		std::array<const AssetsModule::Animation*, static_cast<size_t>(State::Count)> mClips{};

		// Physics thread only.
		State mState = State::Idle;
		double mSimTime = 0.0;
		double mStateStart = 0.0;
		float mYaw = 0.f;
		float mFallSpeed = 0.f;
		JPH::BodyID mHeld; // invalid when carrying nothing

		// Handed from the physics thread to the main thread.
		// The last few steps' poses, oldest first. Several, not two: a frame that comes late, or a physics step that
		// does under load, still finds the two steps around the moment it draws.
		static constexpr size_t kPoseHistory = 8;
		std::mutex mPoseMutex;
		std::array<Pose, kPoseHistory> mPoses{};
		size_t mPoseCount = 0;
		void pushPose(const Pose& pose); // under mPoseMutex
		std::atomic<State> mPublishedState{ State::Idle };
		std::atomic<float> mClearDistance{ 4.5f }; // how far the camera can be before something is in the way

		// Main thread only.
		State mAppliedState = State::Idle;

		// Written by input and the main thread, read by the physics thread.
		std::atomic<bool> mActive{ false };
		std::atomic<bool> mToggle{ false };
		std::atomic<bool> mViewToggle{ false }; // V: first person and back
		bool mFirstPerson = false;              // main thread
		// Main thread: the feet last frame and how far they have gone since the last step.
		Math::Vec3 mStepFeet{};
		float mStride = 0.f;
		bool mStepFeetValid = false;
		std::atomic<bool> mJump{ false };
		std::atomic<bool> mGrab{ false };
		std::atomic<bool> mThrow{ false };
		std::atomic<bool> mKick{ false };
		std::atomic<float> mCameraYaw{ 0.f };
		std::atomic<float> mCameraPitch{ -12.f };
		std::atomic<float> mCameraDistance{ 4.5f };
		std::atomic<float> mMouseX{ 0.f };
		std::atomic<float> mMouseY{ 0.f };
		std::atomic<float> mScroll{ 0.f };
		static constexpr int kKeyCount = 512;
		std::array<std::atomic<bool>, kKeyCount> mPressed{};
	};
}
