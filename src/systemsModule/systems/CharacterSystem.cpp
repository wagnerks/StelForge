#include "CharacterSystem.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "CameraSystem.h"
#include "AppearanceSystem.h"
#include "SoundscapeSystem.h"
#include "RenderSystem.h"
#include "componentsModule/MeshComponent.h"
#include "PhysicsSystem.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/CharacterComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/WaterComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "systemsModule/TasksManager.h"

namespace SFE::SystemsModule {
	namespace {
		constexpr float kRadius = 0.3f;
		constexpr float kCapsuleHalfHeight = 0.6f; // the cylinder part: with the caps, 1.8 m tall
		constexpr float kGravity = -9.81f;
		constexpr float kEyeHeight = 1.45f;
		constexpr float kFirstPersonEyeHeight = 1.62f;
		constexpr float kMouseSensitivity = 0.15f;
		constexpr float kCameraAboveWater = 0.35f;
		constexpr double kInterpolationDelay = 1.25 / 60.0; // a physics step, and a little margin for one that runs late
		constexpr float kTurnEase = 12.f;      // 1/s: the share of the remaining turn made per second
		constexpr float kMaxTurnRate = 600.f;  // degrees per second
		// Loose bodies: where a carried one is held, in front of the chest; how far it may fall behind before it is let
		// go; how hard a throw and a kick send things.
		constexpr float kHoldAhead = 0.75f;
		constexpr float kHoldHeight = 1.15f;
		constexpr float kHoldSlip = 1.5f;
		constexpr float kThrowSpeed = 11.f;
		constexpr float kKickSpeed = 6.f;

		// The columns of the rotation for these Euler degrees: [0] right, [1] up, [2] backward.
		Math::Mat4 rotation(const Math::Vec3& degrees) {
			return Math::Quaternion<float>::fromEulerDegrees(degrees).toMat4();
		}

		Math::Vec3 backward(float pitch, float yaw) {
			const auto m = rotation({ pitch, yaw, 0.f });
			return { m[2][0], m[2][1], m[2][2] };
		}

		// How far `to` is from `from` the short way round, in (-180, 180]. Degrees.
		float angleBetween(float from, float to) {
			return std::fmod(std::fmod(to - from, 360.f) + 540.f, 360.f) - 180.f;
		}

		void addTo(std::atomic<float>& value, float delta) {
			float current = value.load(std::memory_order_relaxed);
			while (!value.compare_exchange_weak(current, current + delta, std::memory_order_relaxed)) {
			}
		}

		constexpr std::string_view kClipNames[] = { "Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop", "Jump_Start", "Jump_Loop", "Jump_Land" };

		// Some exporters prefix the armature ("Armature|Idle_Loop"); the name after it has to match whole, or
		// "Idle_Loop" would find "Crouch_Idle_Loop" first.
		const AssetsModule::Animation* findClip(const AssetsModule::Model* model, std::string_view name) {
			if (!model) {
				return nullptr;
			}
			for (const auto& animation : model->getAnimations()) {
				std::string_view full = animation.getName();
				if (const auto bar = full.rfind('|'); bar != std::string_view::npos) {
					full = full.substr(bar + 1);
				}
				if (full == name) {
					return &animation;
				}
			}
			return nullptr;
		}

		float clipSeconds(const AssetsModule::Animation* animation) {
			return animation ? animation->getDuration() / ComponentsModule::AnimationComponent::ticksPerSecond(*animation) : 0.f;
		}
	}

	CharacterSystem::CharacterSystem() {
		auto* physics = ECSHandler::getSystem<PhysicsSystem>();
		if (!physics) {
			return;
		}
		auto& registry = ECSHandler::registry();
		Math::Vec3 start;
		std::string animations;
		for (const auto& [id, character, transform] : registry.view<const ComponentsModule::CharacterComponent, const TransformComponent>()) {
			if (character && transform) {
				mEntity = id;
				animations = character->animations;
				start = transform->getPos();
				mYaw = transform->getRotate().y;
				mWalkSpeed = character->walkSpeed;
				mRunSpeed = character->runSpeed;
				mSprintSpeed = character->sprintSpeed;
				mJumpHeight = character->jumpHeight;
				break;
			}
		}
		if (mEntity == ecss::INVALID_ID) {
			return;
		}
		for (const auto& [id, water, transform] : registry.view<const ComponentsModule::WaterComponent, const TransformComponent>()) {
			if (water && transform) {
				mHasWater = true;
				mWaterCentre = transform->getPos(true);
				mWaterHalfSize = water->size * 0.5f;
				break;
			}
		}
		if (const auto model = registry.pinComponent<const ModelComponent>(mEntity)) {
			const auto* asset = AssetsModule::ModelLoader::instance()->load(animations.empty() ? model->mPath : animations);
			for (size_t i = 0; i < mClips.size(); ++i) {
				mClips[i] = findClip(asset, kClipNames[i]);
			}
		}
		// A model with no clips of its own was given no animation to play them with.
		if (!registry.pinComponent<const ComponentsModule::AnimationComponent>(mEntity)) {
			registry.addComponent<ComponentsModule::AnimationComponent>(mEntity);
		}

		JPH::CharacterVirtualSettings settings;
		settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.f);
		// The capsule stands on the character's position, so the position is where its feet are.
		settings.mShape = JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0.f, kCapsuleHalfHeight + kRadius, 0.f), JPH::Quat::sIdentity(),
			new JPH::CapsuleShape(kCapsuleHalfHeight, kRadius)).Create().Get();
		settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -kRadius);
		mBody = std::make_unique<JPH::CharacterVirtual>(&settings, JPH::RVec3(start.x, start.y, start.z), JPH::Quat::sIdentity(), &physics->world());

		pushPose({ start, mYaw, Engine::clockSeconds() });
		mCameraYaw.store(mYaw + 180.f);
		physics->addStepCallback([this](float dt) { step(dt); });

		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			const int k = static_cast<int>(key);
			if (k >= 0 && k < kKeyCount && type != CoreModule::InputEventType::REPEAT) {
				mPressed[static_cast<size_t>(k)].store(type == CoreModule::InputEventType::PRESS, std::memory_order_relaxed);
			}
			if (type == CoreModule::InputEventType::PRESS) {
				if (key == CoreModule::InputKey::KEY_INSERT) {
					mToggle.store(true, std::memory_order_relaxed);
				}
				if (key == CoreModule::InputKey::KEY_V) {
					mViewToggle.store(true, std::memory_order_relaxed);
				}
				if (key == CoreModule::InputKey::KEY_SPACE) {
					mJump.store(true, std::memory_order_relaxed);
				}
				if (key == CoreModule::InputKey::KEY_E) {
					mGrab.store(true, std::memory_order_relaxed);
				}
				if (key == CoreModule::InputKey::KEY_F) {
					mKick.store(true, std::memory_order_relaxed);
				}
			}
		};
		onMouseBtnEvent = [this](Math::DVec2, CoreModule::MouseButton button, CoreModule::InputEventType type) {
			if (button == CoreModule::MouseButton::MOUSE_BUTTON_1 && type == CoreModule::InputEventType::PRESS) {
				mThrow.store(true, std::memory_order_relaxed);
			}
		};
		onMouseEvent = [this](Math::DVec2, Math::DVec2 offset) {
			addTo(mMouseX, static_cast<float>(offset.x));
			addTo(mMouseY, static_cast<float>(offset.y));
		};
		onScrollEvent = [this](Math::DVec2 offset) {
			addTo(mScroll, static_cast<float>(offset.y));
		};

		// Standing still from the first frame, not faded in from whatever clip the model loaded with.
		if (auto animation = registry.pinComponent<ComponentsModule::AnimationComponent>(mEntity)) {
			animation->crossfade(mClips[static_cast<size_t>(State::Idle)], true, 0.f, Engine::instance()->animationClock());
			TasksManager::instance()->notify({ mEntity, ANIMATION_UPDATED });
		}
		setActive(true);
	}

	CharacterSystem::~CharacterSystem() = default;

	bool CharacterSystem::keyDown(CoreModule::InputKey key) const {
		const int k = static_cast<int>(key);
		return k >= 0 && k < kKeyCount && mPressed[static_cast<size_t>(k)].load(std::memory_order_relaxed);
	}

	float CharacterSystem::waterDepthAt(const Math::Vec3& point) const {
		if (!mHasWater || std::fabs(point.x - mWaterCentre.x) > mWaterHalfSize || std::fabs(point.z - mWaterCentre.z) > mWaterHalfSize) {
			return 0.f;
		}
		return mWaterCentre.y - point.y;
	}

	// ===== Physics thread =====================================================

	CharacterSystem::State CharacterSystem::nextState(bool onGround, float speed, float fall) const {
		const float inState = static_cast<float>(mSimTime - mStateStart);
		if (!onGround) {
			// A step off a small bump is not a fall: only a real drop, or the jump running out, goes airborne.
			const bool airborne = mState == State::JumpStart
				? inState >= clipSeconds(mClips[static_cast<size_t>(State::JumpStart)])
				: fall > 2.5f;
			return airborne ? State::Airborne : mState;
		}
		const float walkRun = 0.5f * (mWalkSpeed + mRunSpeed);
		const float runSprint = 0.5f * (mRunSpeed + mSprintSpeed);
		const State moving = speed < 0.3f ? State::Idle : speed < walkRun ? State::Walk : speed < runSprint ? State::Run : State::Sprint;
		if (mState == State::JumpStart && inState < 0.2f) {
			return mState; // just jumped: the ground state catches up with the take-off a step or two later
		}
		if (mState == State::Airborne || mState == State::JumpStart) {
			return fall > 4.f ? State::Land : moving;
		}
		if (mState == State::Land && inState < 0.35f && speed < 1.f) {
			return mState;
		}
		return moving;
	}

	void CharacterSystem::step(float dt) {
		auto* physics = ECSHandler::getSystem<PhysicsSystem>();
		mSimTime += dt;
		const bool active = mActive.load(std::memory_order_relaxed);

		// What the keys ask for, on the ground plane and relative to where the camera looks.
		const float cameraYaw = mCameraYaw.load(std::memory_order_relaxed);
		const auto camera = rotation({ 0.f, cameraYaw, 0.f });
		const Math::Vec3 forward{ -camera[2][0], 0.f, -camera[2][2] };
		const Math::Vec3 right{ camera[0][0], 0.f, camera[0][2] };
		const auto axis = [&](CoreModule::InputKey plus, CoreModule::InputKey minus) {
			return active ? (keyDown(plus) ? 1.f : 0.f) - (keyDown(minus) ? 1.f : 0.f) : 0.f;
		};
		Math::Vec3 wish = forward * axis(CoreModule::InputKey::KEY_W, CoreModule::InputKey::KEY_S)
			+ right * axis(CoreModule::InputKey::KEY_D, CoreModule::InputKey::KEY_A);
		const float wishLength = Math::length(wish);
		if (wishLength > 1e-3f) {
			wish = wish / wishLength;
		}

		float speed = mRunSpeed;
		if (keyDown(CoreModule::InputKey::KEY_LEFT_SHIFT)) {
			speed = mSprintSpeed;
		}
		else if (keyDown(CoreModule::InputKey::KEY_LEFT_CONTROL)) {
			speed = mWalkSpeed;
		}
		const auto feet = mBody->GetPosition();
		// Wading: the deeper the water, the slower, down to under half the speed once it is over the waist.
		const float depth = waterDepthAt({ static_cast<float>(feet.GetX()), static_cast<float>(feet.GetY()), static_cast<float>(feet.GetZ()) });
		speed *= 1.f - 0.55f * std::clamp(depth - 0.2f, 0.f, 1.f);

		const bool onGround = mBody->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
		const JPH::Vec3 current = mBody->GetLinearVelocity();
		const JPH::Vec3 desired(wish.x * speed, 0.f, wish.z * speed);
		JPH::Vec3 horizontal(current.GetX(), 0.f, current.GetZ());
		// Quick to respond on the ground, barely steerable in the air.
		horizontal += (desired - horizontal) * (std::min)(1.f, dt * (onGround ? 12.f : 1.5f));

		float vertical = onGround ? 0.f : current.GetY();
		const bool jump = mJump.exchange(false) && active;
		if (onGround && jump) {
			vertical = std::sqrt(2.f * -kGravity * mJumpHeight);
			mState = State::JumpStart;
			mStateStart = mSimTime;
		}
		vertical += kGravity * dt;
		mBody->SetLinearVelocity(horizontal + JPH::Vec3(0.f, vertical, 0.f) + (onGround ? mBody->GetGroundVelocity() : JPH::Vec3::sZero()));

		const Math::Vec3 feetPos{ static_cast<float>(feet.GetX()), static_cast<float>(feet.GetY()), static_cast<float>(feet.GetZ()) };
		handleBodies(dt, feetPos, -backward(mCameraPitch.load(std::memory_order_relaxed), cameraYaw));

		const JPH::CharacterVirtual::ExtendedUpdateSettings settings;
		const JPH::DefaultBroadPhaseLayerFilter broadPhase(physics->broadPhaseFilter(), PhysicsSystem::kMoving);
		const JPH::DefaultObjectLayerFilter layers(physics->layerPairFilter(), PhysicsSystem::kMoving);
		// What is carried does not get in the way of the one carrying it.
		const JPH::IgnoreSingleBodyFilter notHeld(mHeld);
		mBody->ExtendedUpdate(dt, JPH::Vec3(0.f, kGravity, 0.f), settings, broadPhase, layers, notHeld, {}, physics->tempAllocator());

		if (wishLength > 1e-3f) {
			// Eased towards where it is going: quick through most of a turn, settling gently at the end, and no faster
			// than a person swings round.
			const float remaining = angleBetween(mYaw, std::atan2(wish.x, wish.z) * 57.29578f);
			const float maxStep = kMaxTurnRate * dt;
			mYaw += std::clamp(remaining * (1.f - std::exp(-kTurnEase * dt)), -maxStep, maxStep);
		}

		const auto velocity = mBody->GetLinearVelocity();
		const bool grounded = mBody->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
		if (!grounded) {
			mFallSpeed = (std::max)(mFallSpeed, -velocity.GetY());
		}
		const State next = nextState(grounded, std::sqrt(velocity.GetX() * velocity.GetX() + velocity.GetZ() * velocity.GetZ()), mFallSpeed);
		if (grounded && next != State::JumpStart) {
			mFallSpeed = 0.f;
		}
		if (next != mState) {
			mState = next;
			mStateStart = mSimTime;
		}
		mPublishedState.store(mState, std::memory_order_relaxed);

		const auto position = mBody->GetPosition();
		const Math::Vec3 pose{ static_cast<float>(position.GetX()), static_cast<float>(position.GetY()), static_cast<float>(position.GetZ()) };

		// The camera's reach, tested here with the world rather than from the main thread while it steps: pulled in
		// front of whatever stands between the character and the camera, so a hill never hides it.
		const float wanted = mCameraDistance.load(std::memory_order_relaxed);
		const auto back = backward(mCameraPitch.load(std::memory_order_relaxed), cameraYaw);
		const Math::Vec3 eye{ pose.x, pose.y + kEyeHeight, pose.z };
		float clear = wanted;
		if (const auto hit = physics->castRay(eye, back * (wanted + 0.3f))) {
			clear = (std::max)(0.4f, *hit * (wanted + 0.3f) - 0.3f);
		}
		mClearDistance.store(clear, std::memory_order_relaxed);

		std::lock_guard lock(mPoseMutex);
		pushPose({ pose, mYaw, physics->stepTime() });
	}

	std::vector<JPH::BodyID> CharacterSystem::bodiesNear(const Math::Vec3& centre, const Math::Vec3& halfExtent) const {
		auto* physics = ECSHandler::getSystem<PhysicsSystem>();
		auto& world = physics->world();
		JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> hits;
		const JPH::Vec3 c(centre.x, centre.y, centre.z);
		const JPH::Vec3 h(halfExtent.x, halfExtent.y, halfExtent.z);
		world.GetBroadPhaseQuery().CollideAABox(JPH::AABox(c - h, c + h), hits,
			JPH::DefaultBroadPhaseLayerFilter(physics->broadPhaseFilter(), PhysicsSystem::kMoving),
			JPH::DefaultObjectLayerFilter(physics->layerPairFilter(), PhysicsSystem::kMoving));
		std::vector<JPH::BodyID> found;
		auto& bodies = world.GetBodyInterfaceNoLock();
		for (const auto& id : hits.mHits) {
			if (bodies.GetMotionType(id) == JPH::EMotionType::Dynamic) {
				found.push_back(id);
			}
		}
		return found;
	}

	void CharacterSystem::handleBodies(float dt, const Math::Vec3& feet, const Math::Vec3& look) {
		const bool active = mActive.load(std::memory_order_relaxed);
		const bool grab = mGrab.exchange(false) && active;
		const bool toss = mThrow.exchange(false) && active;
		const bool kick = mKick.exchange(false) && active;
		auto& bodies = ECSHandler::getSystem<PhysicsSystem>()->world().GetBodyInterfaceNoLock();
		const float yaw = mYaw * 0.017453292f;
		const Math::Vec3 facing{ std::sin(yaw), 0.f, std::cos(yaw) };
		const auto toVec = [](const JPH::RVec3& v) {
			return Math::Vec3{ static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()) };
		};

		if (grab) {
			if (!mHeld.IsInvalid()) {
				mHeld = {};
			}
			else {
				// The nearest thing within reach in front, from the ground up to the chest.
				const Math::Vec3 reach = feet + facing * 0.7f + Math::Vec3{ 0.f, 0.5f, 0.f };
				float nearest = std::numeric_limits<float>::max();
				for (const auto& id : bodiesNear(reach, { 0.7f, 0.8f, 0.7f })) {
					const float d = Math::length(toVec(bodies.GetCenterOfMassPosition(id)) - reach);
					if (d < nearest) {
						nearest = d;
						mHeld = id;
					}
				}
			}
		}

		if (!mHeld.IsInvalid()) {
			const Math::Vec3 hold = feet + facing * kHoldAhead + Math::Vec3{ 0.f, kHoldHeight, 0.f };
			const Math::Vec3 at = toVec(bodies.GetCenterOfMassPosition(mHeld));
			const Math::Vec3 gap = hold - at;
			if (Math::length(gap) > kHoldSlip) {
				mHeld = {}; // caught on something
			}
			else if (toss) {
				const Math::Vec3 aim = Math::normalize(look + Math::Vec3{ 0.f, 0.25f, 0.f }) * kThrowSpeed;
				const auto carried = mBody->GetLinearVelocity();
				bodies.SetLinearVelocity(mHeld, JPH::Vec3(aim.x, aim.y, aim.z) + JPH::Vec3(carried.GetX(), 0.f, carried.GetZ()));
				bodies.SetAngularVelocity(mHeld, JPH::Vec3(facing.z, 0.f, -facing.x) * 6.f);
				mHeld = {};
			}
			else {
				// Pulled to the hold point quickly but not in one step, so it swings a little and still collides.
				Math::Vec3 velocity = gap * (0.5f / dt);
				const float speed = Math::length(velocity);
				if (speed > 12.f) {
					velocity = velocity * (12.f / speed);
				}
				bodies.SetLinearVelocity(mHeld, JPH::Vec3(velocity.x, velocity.y, velocity.z));
				bodies.SetAngularVelocity(mHeld, bodies.GetAngularVelocity(mHeld) * 0.8f);
				bodies.ActivateBody(mHeld);
			}
		}

		if (kick) {
			const Math::Vec3 foot = feet + facing * 0.55f + Math::Vec3{ 0.f, 0.3f, 0.f };
			for (const auto& id : bodiesNear(foot, { 0.5f, 0.35f, 0.5f })) {
				if (id == mHeld) {
					continue;
				}
				// Light things fly, heavy ones only shift.
				JPH::BodyLockRead lock(ECSHandler::getSystem<PhysicsSystem>()->world().GetBodyLockInterfaceNoLock(), id);
				const float inverseMass = lock.Succeeded() ? lock.GetBody().GetMotionPropertiesUnchecked()->GetInverseMass() : 1.f;
				const float strength = std::clamp(std::sqrt(inverseMass * 0.3f), 0.25f, 1.f);
				const Math::Vec3 push = (facing + Math::Vec3{ 0.f, 0.45f, 0.f }) * kKickSpeed * strength;
				bodies.AddLinearVelocity(id, JPH::Vec3(push.x, push.y, push.z));
				bodies.AddAngularImpulse(id, JPH::Vec3(facing.z, 0.3f, -facing.x) * (0.02f * strength));
			}
		}
	}

	// ===== Main thread ========================================================

	void CharacterSystem::pushPose(const Pose& pose) {
		if (mPoseCount == mPoses.size()) {
			std::move(mPoses.begin() + 1, mPoses.end(), mPoses.begin());
			--mPoseCount;
		}
		mPoses[mPoseCount++] = pose;
	}

	void CharacterSystem::setActive(bool active) {
		mActive.store(active, std::memory_order_relaxed);
		if (auto* cameras = ECSHandler::getSystem<CameraSystem>()) {
			cameras->setUserControl(!active);
		}
		// The cursor is held while playing, so the mouse turns the camera without leaving the window.
		if (auto* window = Engine::instance()->getMainWindow(); window && !Engine::instance()->capturingFrame()) {
			glfwSetInputMode(window, GLFW_CURSOR, active ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		}
		mMouseX.store(0.f);
		mMouseY.store(0.f);
	}

	void CharacterSystem::update(float dt) {
		if (!mBody) {
			return;
		}
		if (mToggle.exchange(false)) {
			setActive(!mActive.load(std::memory_order_relaxed));
		}
		if (!mActive.load(std::memory_order_relaxed)) {
			return;
		}
		if (mViewToggle.exchange(false)) {
			setFirstPerson(!mFirstPerson);
		}

		// A capture holds still whatever the mouse does over its window, as the free camera does.
		const float look = Engine::instance()->capturingFrame() ? 0.f : kMouseSensitivity;
		mCameraYaw.store(mCameraYaw.load() - mMouseX.exchange(0.f) * look);
		mCameraPitch.store(std::clamp(mCameraPitch.load() + mMouseY.exchange(0.f) * look, -70.f, 35.f));
		mCameraDistance.store(std::clamp(mCameraDistance.load() - mScroll.exchange(0.f) * (look > 0.f ? 0.5f : 0.f), 1.5f, 12.f));

		// The pose as it was a little in the past, between the two steps around that moment: behind by that much, but it
		// moves every frame instead of in 60 Hz jumps, and a late step does not stall it.
		const double shown = Engine::clockSeconds() - kInterpolationDelay;
		Pose from;
		Pose to;
		{
			std::lock_guard lock(mPoseMutex);
			size_t i = 0;
			while (i + 2 < mPoseCount && mPoses[i + 1].at <= shown) {
				++i;
			}
			from = mPoses[i];
			to = mPoses[(std::min)(i + 1, mPoseCount - 1)];
		}
		const double span = to.at - from.at;
		const float t = span > 1e-6 ? static_cast<float>(std::clamp((shown - from.at) / span, 0.0, 1.0)) : 1.f;
		const Math::Vec3 feet = from.feet + (to.feet - from.feet) * t;
		const float yaw = from.yaw + angleBetween(from.yaw, to.yaw) * t;
		if (auto transform = ECSHandler::registry().pinComponent<TransformComponent>(mEntity)) {
			transform->setPos(feet);
			transform->setRotate({ 0.f, yaw, 0.f });
			// Now, not on the transform worker: the camera placed below is applied at once, and a character whose
			// matrix arrived a frame later now and then would shake against it.
			transform->reloadTransform();
			TasksManager::instance()->notify({ mEntity, TRAHSFORM_RELOADED, transform.get() });
		}

		const State state = mPublishedState.load(std::memory_order_relaxed);
		bool landed = false;
		if (state != mAppliedState) {
			landed = state == State::Land;
			mAppliedState = state;
			const bool loop = state != State::JumpStart && state != State::Land;
			const auto* clip = mClips[static_cast<size_t>(state)];
			if (auto animation = ECSHandler::registry().pinComponent<ComponentsModule::AnimationComponent>(mEntity); animation && clip) {
				animation->crossfade(clip, loop, state == State::JumpStart ? 0.08f : 0.2f, Engine::instance()->animationClock());
				TasksManager::instance()->notify({ mEntity, ANIMATION_UPDATED });
			}
		}

		// A step each stride while walking on the ground, longer strides the faster it goes; landing is a step too.
		if (auto* soundscape = ECSHandler::getSystem<SoundscapeSystem>()) {
			const bool walking = mAppliedState == State::Walk || mAppliedState == State::Run || mAppliedState == State::Sprint;
			if (mStepFeetValid && dt > 0.f) {
				const float dx = feet.x - mStepFeet.x;
				const float dz = feet.z - mStepFeet.z;
				const float moved = std::sqrt(dx * dx + dz * dz);
				const float speed = moved / dt;
				if (walking && moved < 2.f) {
					mStride += moved;
					const float stride = std::clamp(0.55f + 0.17f * speed, 0.65f, 1.6f);
					if (mStride >= stride) {
						mStride -= stride;
						soundscape->footstep(feet, speed);
					}
				}
				else {
					mStride = 0.f;
				}
			}
			if (landed) {
				soundscape->footstep(feet, 4.f);
			}
		}
		mStepFeet = feet;
		mStepFeetValid = true;

		follow(feet);
	}

	void CharacterSystem::setFirstPerson(bool firstPerson) {
		mFirstPerson = firstPerson;
		// The character is not drawn from inside it; back in third person the appearance puts its unused hairstyles
		// away again.
		if (auto mesh = ECSHandler::registry().pinComponent<MeshComponent>(mEntity)) {
			for (auto& material : mesh->meshMaterials) {
				material.hidden = firstPerson;
			}
		}
		if (!firstPerson) {
			if (auto* appearance = ECSHandler::getSystem<AppearanceSystem>()) {
				appearance->reapply(mEntity);
			}
		}
		if (auto* renderSystem = ECSHandler::getSystem<RenderSystem>()) {
			renderSystem->scene().markEntity(mEntity);
		}
	}

	void CharacterSystem::follow(const Math::Vec3& feet) {
		auto* cameras = ECSHandler::getSystem<CameraSystem>();
		if (!cameras) {
			return;
		}
		const float pitch = mCameraPitch.load(std::memory_order_relaxed);
		const float yaw = mCameraYaw.load(std::memory_order_relaxed);
		const Math::Vec3 target{ feet.x, feet.y + kEyeHeight, feet.z };
		if (mFirstPerson) {
			// From the eyes, a little ahead of them so the near plane stays clear of the head.
			const Math::Vec3 eye = Math::Vec3{ feet.x, feet.y + kFirstPersonEyeHeight, feet.z } - backward(pitch, yaw) * 0.12f;
			cameras->placeCamera(eye, { pitch, yaw, 0.f });
			return;
		}
		const float distance = (std::min)(mCameraDistance.load(std::memory_order_relaxed), mClearDistance.load(std::memory_order_relaxed));
		Math::Vec3 eye = target + backward(pitch, yaw) * distance;
		Math::Vec3 turn{ pitch, yaw, 0.f };
		// The water is only drawn from above: when the character wades in deep, the camera stays over the surface
		// and looks down at it through the water.
		const float depth = waterDepthAt(eye);
		if (depth > -kCameraAboveWater) {
			eye.y += depth + kCameraAboveWater;
			const Math::Vec3 toTarget = target - eye;
			const float flat = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
			turn.x = std::atan2(toTarget.y, (std::max)(flat, 0.01f)) * 57.29578f;
		}
		cameras->placeCamera(eye, turn);
	}
}
