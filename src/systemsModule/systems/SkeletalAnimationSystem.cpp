#include "SkeletalAnimationSystem.h"

#include "debugModule/Benchmark.h"

namespace SFE::SystemsModule {
	void SkeletalAnimationSystem::update(float) {
		FUNCTION_BENCHMARK;
		// Pose is evaluated on GPU in AnimationComputePass.
	}
}
