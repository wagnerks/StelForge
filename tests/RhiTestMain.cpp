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
	SFE::Render::setRhiLogSink(SFE::Render::Tests::captureRhiLog);
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new SFE::Render::Tests::GlfwEnvironment());
	return RUN_ALL_TESTS();
}
