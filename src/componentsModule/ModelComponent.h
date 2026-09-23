#pragma once

#include <algorithm>
#include <array>
#include <string_view>
#include <cmath>
#include <limits>
#include <shared_mutex>
#include <vector>

#include "ComponentBase.h"
#include "assetsModule/modelModule/Animation.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/modelModule/Model.h"
#include "ecss/Types.h"


namespace SFE::ComponentsModule {
	struct LODData {
		size_t mLodLevel = 0;
		float mCurrentLodValue = 0.f;

		void setLodLevel(size_t lodLevel);
		size_t getLodLevel() const;

		const std::vector<float>& getLodLevelValues();
		void addLodLevelValue(float lodLevel);

		float getCurrentLodValue() const;
		void setCurrentLodValue(float currentLodValue);

		std::vector<float> mLodLevelValues;
	};

	struct AABBComponent {
		AABBComponent() = default;
		AABBComponent(const AABBComponent& other)
			: aabbs(other.aabbs) {}

		AABBComponent(AABBComponent&& other) noexcept
			: aabbs(std::move(other.aabbs)) {}

		AABBComponent& operator=(const AABBComponent& other) {
			if (this == &other)
				return *this;
			aabbs = other.aabbs;
			return *this;
		}

		AABBComponent& operator=(AABBComponent&& other) noexcept {
			if (this == &other)
				return *this;
			aabbs = std::move(other.aabbs);
			return *this;
		}

		std::vector<FrustumModule::AABB> aabbs;
		std::vector<FrustumModule::AABB> defaultAabbs;
		float aabbScale = 1.f;
		std::shared_mutex mtx;
	};

	class ModelComponent : public ecss::ComponentInterface {
	public:
		ModelComponent(ecss::SectorId id) : ComponentInterface(id) {};
		void init(AssetsModule::Model* model) {
			mPath = model->assetPath;
			addMeshData(model->getLODs());
		}

		const AssetsModule::Model::LOD& getModel() const;
		AssetsModule::Model::LOD& getModel(size_t LOD) const;
		const AssetsModule::Model::LOD& getModelLowestDetails() const;

		void setModel(std::vector<AssetsModule::Model::LOD>* data);
		void addMeshData(std::vector<AssetsModule::Model::LOD>* meshData);
		LODData mLOD;

		std::string mPath = "";

		AssetsModule::Armature armature;
		std::vector<SFE::Math::Mat4> boneMatrices;
	private:
		std::vector<AssetsModule::Model::LOD>* mModel = nullptr;
	};

	// The pose is evaluated on the GPU from these fields and Engine::animationClock(), so nothing advances per frame.
	// Whoever changes them sends TaskType::ANIMATION_UPDATED, or the GPU keeps the old state.
	struct AnimationComponent {
		AnimationComponent() = default;

		bool mPlay = true;
		bool mLoop = true;

		const AssetsModule::Animation* mCurrentAnimation = nullptr;

		// While playing: the clock value at which the clip was at tick 0, NaN until the first frame that draws it.
		// While stopped, or before that frame: the tick it stands at.
		double mStart = std::numeric_limits<double>::quiet_NaN();
		float mStoppedTime = 0.f;

		static float ticksPerSecond(const AssetsModule::Animation& animation) {
			const auto tps = animation.getTicksPerSecond();
			return tps > 0.f ? tps : 25.f;
		}

		// The same formula animateBones.cs uses.
		float time(double clock) const {
			if (!mCurrentAnimation) {
				return 0.f;
			}
			if (!mPlay || std::isnan(mStart)) {
				return mStoppedTime;
			}
			const auto duration = mCurrentAnimation->getDuration();
			auto t = static_cast<float>((clock - mStart) * ticksPerSecond(*mCurrentAnimation));
			if (duration <= 0.f) {
				return t;
			}
			return mLoop ? t - duration * std::floor(t / duration) : (std::min)(t, duration);
		}

		void setTime(float ticks, double clock) {
			mStoppedTime = ticks;
			if (mCurrentAnimation) {
				mStart = clock - ticks / ticksPerSecond(*mCurrentAnimation);
			}
		}

		// Plays from `ticks` starting with the first frame that draws it. For code off the main thread, where the
		// clock cannot be read, and so the start does not depend on when a loader thread ran.
		void play(float ticks) {
			mPlay = true;
			mStoppedTime = ticks;
			mStart = std::numeric_limits<double>::quiet_NaN();
		}

		void setPlaying(bool play, double clock) {
			const auto t = time(clock);
			mPlay = play;
			setTime(t, clock);
		}

		// A turn laid over the clip, as a head follows where the eyes look: mLook in the model's space, shared out
		// along mLookBones, each a bone's name and how much of the turn it makes about its own joint (a child's share
		// adds to what its parent has made). No bone named, no turn. The names are kept as pointers, so they must
		// outlive the component: string literals.
		struct LookBone {
			const char* name = nullptr;
			float share = 0.f;
		};
		Math::Quaternion<float> mLook;
		std::array<LookBone, 4> mLookBones{};

		// Set while something poses this skeleton itself, bone by bone, into ArmatureBonesComponent: an arm reaching
		// for what it takes hold of, fingers closing on it. The clip is then only what that pose is built on -- it is
		// read where the poser samples it and not on the device, so AnimationComputePass leaves the entity alone and
		// the bones written by hand are the ones drawn. Costs the pose bounds (the cull falls back to the bind pose)
		// and the motion vectors of the bones, so it is for the few skeletons that need it, not for a crowd.
		bool mPosedByHand = false;

		// The clip being faded out while a crossfade lasts: its own start, loop and play state, frozen at the switch.
		const AssetsModule::Animation* mFromAnimation = nullptr;
		double mFromStart = 0.0;
		float mFromStoppedTime = 0.f;
		bool mFromPlay = true;
		bool mFromLoop = true;
		// The clock value the fade began at, and how long it takes; no fade while the duration is 0.
		double mBlendStart = 0.0;
		float mBlendDuration = 0.f;

		// Switches to `next` from its first tick, blending out of the current clip over `seconds`. Main thread: it
		// reads the clock. Nothing happens when `next` is already playing.
		// With `keepPhase` the new clip starts as far through its own cycle as the old one was through its: that is
		// how a walk becomes a run without the legs snapping back to the clip's first frame, which they do every time
		// two strides are swapped at their beginnings. Wrong for a clip that plays once -- a jump, a landing, a pick
		// up all start where they start -- so it is asked for, not assumed.
		void crossfade(const AssetsModule::Animation* next, bool loop, float seconds, double clock, bool keepPhase = false) {
			if (next == mCurrentAnimation) {
				return;
			}
			mBlendDuration = 0.f;
			mFromAnimation = nullptr;
			if (mCurrentAnimation && (seconds > 0.f || keepPhase)) {
				if (mPlay && std::isnan(mStart)) {
					setTime(mStoppedTime, clock);
				}
			}
			float at = 0.f;
			if (keepPhase && mCurrentAnimation && next) {
				const float was = mCurrentAnimation->getDuration();
				const float becomes = next->getDuration();
				if (was > 0.f && becomes > 0.f) {
					at = time(clock) / was * becomes;
				}
			}
			if (mCurrentAnimation && seconds > 0.f) {
				mFromAnimation = mCurrentAnimation;
				mFromStart = mStart;
				mFromStoppedTime = mStoppedTime;
				mFromPlay = mPlay;
				mFromLoop = mLoop;
				mBlendStart = clock;
				mBlendDuration = seconds;
			}
			mCurrentAnimation = next;
			mLoop = loop;
			mPlay = true;
			setTime(at, clock);
		}
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::AABBComponent> : std::true_type {};
template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::ModelComponent> : std::true_type {};

using SFE::ComponentsModule::ModelComponent;