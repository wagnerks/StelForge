#include <cstdlib>

#include <gtest/gtest.h>

#include "RhiTestFixture.h"

// Same GPU as the runtime: without these OpenGL runs on the integrated GPU of a hybrid laptop while
// Vulkan picks the discrete one, and every cross-backend comparison compares two vendors.
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Own entry point instead of gtest_main: the RHI's log sink has to be installed before any test runs.
int main(int argc, char** argv) {
	// Implicit layers are whatever the machine has installed: overlays and capture hooks. Every test creates and
	// destroys its own Vulkan instance and device, which a game never does, and the Overwolf hook reached through
	// FPS Monitor's layer dereferences null inside the second vkCreateDevice of a process. The tests are about the
	// RHI, so they run without them; the validation layer is enabled explicitly and stays on. Set before the loader
	// is first used, and only when the caller did not choose.
	if (!std::getenv("VK_LOADER_LAYERS_DISABLE")) {
		_putenv_s("VK_LOADER_LAYERS_DISABLE", "~implicit~");
	}
	SFE::Render::setRhiLogSink(SFE::Render::Tests::captureRhiLog);
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new SFE::Render::Tests::GlfwEnvironment());
	return RUN_ALL_TESTS();
}
