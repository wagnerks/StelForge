#include <cmath>
#include <vector>

#include "GameTestFixture.h"
#include "assetsModule/modelModule/Animation.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/WorldTimeSystem.h"

namespace SFE::GameTests {
	using ComponentsModule::AnimationComponent;
	using ComponentsModule::TransformComponent;
	using ComponentsModule::TransformMatComp;

	// Position and rotation set on new entities reach the matrix the renderer reads, through the transform events
	// and TransformSystem's worker. Not a reproducer for the markDirty race fixed alongside this suite: that window
	// is too narrow to hit on demand, and the test passes on the old code too.
	TEST(GameTest, SpawnedTransformsReachTheRenderCopy) {
		auto& registry = ECSHandler::registry();
		std::vector<ecss::EntityId> ids;
		for (int i = 0; i < 256; ++i) {
			const auto id = registry.takeEntity();
			registry.addComponent<TransformComponent>(id, id);
			ids.push_back(id);
		}
		for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
			auto transform = registry.pinComponent<TransformComponent>(ids[i]);
			ASSERT_TRUE(transform);
			transform->setPos({ static_cast<float>(i), 2.f, 3.f });
			transform->setRotate({ 0.f, 90.f, 0.f });
		}

		step(3);

		for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
			const auto render = registry.pinComponent<const TransformMatComp>(ids[i]);
			ASSERT_TRUE(render) << "entity " << ids[i] << " never got a render transform";
			const auto& m = render->mTransform;
			EXPECT_FLOAT_EQ(m[3][0], static_cast<float>(i)) << "entity " << ids[i];
			EXPECT_FLOAT_EQ(m[3][1], 2.f) << "entity " << ids[i];
			// A 90 degree yaw leaves nothing of X on X; the unrotated matrix has 1 there.
			EXPECT_NEAR(m[0][0], 0.f, 1e-5f) << "entity " << ids[i] << " kept its unrotated matrix";
		}
	}

	// With a fixed frame step an animation moves by exactly frames * step * ticks per second.
	TEST(GameTest, AnimationAdvancesByTheFrameStep) {
		auto& registry = ECSHandler::registry();
		// The default world spawns its animated characters from the chunk loader, which is asynchronous.
		ecss::EntityId animated = ecss::INVALID_ID;
		for (int attempt = 0; attempt < 600 && animated == ecss::INVALID_ID; ++attempt) {
			for (auto [id, animation] : registry.view<const AnimationComponent>()) {
				if (animation && animation->mCurrentAnimation && animation->mPlay && animation->mLoop) {
					animated = id;
					break;
				}
			}
			if (animated == ecss::INVALID_ID) {
				step();
			}
		}
		ASSERT_NE(animated, ecss::INVALID_ID) << "no animated entity in the default world";

		float before = 0.f;
		float ticksPerSecond = 0.f;
		float duration = 0.f;
		{
			const auto animation = registry.pinComponent<const AnimationComponent>(animated);
			before = animation->mCurrentTime;
			ticksPerSecond = animation->mCurrentAnimation->getTicksPerSecond();
			duration = animation->mCurrentAnimation->getDuration();
		}
		if (ticksPerSecond <= 0.f) {
			ticksPerSecond = 25.f;
		}

		constexpr int kFrames = 30;
		step(kFrames);

		// Summed frame by frame the way the pass does, so float rounding matches.
		float expected = before;
		for (int i = 0; i < kFrames; ++i) {
			expected += ticksPerSecond * kFrameTime;
			if (duration > 0.f) {
				expected = std::fmod(expected, duration);
			}
		}
		const auto animation = registry.pinComponent<const AnimationComponent>(animated);
		EXPECT_FLOAT_EQ(animation->mCurrentTime, expected);
	}

	// Tick systems step from the frame under a fixed step: world time (1 tick a second) moves in whole seconds.
	TEST(GameTest, WorldTimeTicksFromTheFrameStep) {
		auto* worldTime = ECSHandler::getSystem<SystemsModule::WorldTimeSystem>();
		ASSERT_NE(worldTime, nullptr);
		const float before = worldTime->getWorldTime();
		step(125);
		const float advanced = worldTime->getWorldTime() - before;
		// 125 frames are 2.08 s; what the accumulator held before adds at most one more tick.
		EXPECT_TRUE(advanced == 2.f || advanced == 3.f) << "advanced " << advanced;
		EXPECT_FLOAT_EQ(std::fmod(advanced, 1.f), 0.f);
	}
}
