#include <gtest/gtest.h>

#include "GameTestFixture.h"
#include "core/NativeLibraries.h"

// Own entry point: the native DLLs (assimp, jsoncpp) are delay-loaded out of <exe>/ThirdParty and the loader has to
// be told before anything touches them, and the engine is one per process, so it lives in a global environment.
int main(int argc, char** argv) {
	SFE::NativeLibraries::init();
	testing::InitGoogleTest(&argc, argv);
	testing::AddGlobalTestEnvironment(new SFE::GameTests::GameEnvironment);
	return RUN_ALL_TESTS();
}
