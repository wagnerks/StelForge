#include <gtest/gtest.h>

#include "core/NativeLibraries.h"
#include "RhiTestFixture.h"

// Own entry point instead of gtest_main, for the same reason the two apps have one: the native
// DLLs are delay-loaded out of <exe>/ThirdParty, and that directory has to be handed to the
// loader before anything touches assimp or jsoncpp.
int main(int argc, char** argv) {
	SFE::NativeLibraries::init();
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new SFE::Render::Tests::GlfwEnvironment());
	return RUN_ALL_TESTS();
}
