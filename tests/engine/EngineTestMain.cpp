#include <gtest/gtest.h>

#include "EngineTestFixture.h"
#include "core/NativeLibraries.h"

#include <string_view>

#ifdef _WIN32
namespace SFE::EngineTests {
	int crashForTest(std::string_view way);
}
#endif

// Own entry point: the native DLLs (assimp, jsoncpp) are delay-loaded out of <exe>/ThirdParty and the loader has to
// be told before anything touches them, and the engine is one per process, so it lives in a global environment.
int main(int argc, char** argv) {
	SFE::NativeLibraries::init();
#ifdef _WIN32
	// A child of the crash report test, there to crash one way.
	if (argc == 3 && std::string_view(argv[1]) == "--crash") {
		return SFE::EngineTests::crashForTest(argv[2]);
	}
#endif
	testing::InitGoogleTest(&argc, argv);
	testing::AddGlobalTestEnvironment(new SFE::EngineTests::EngineEnvironment);
	return RUN_ALL_TESTS();
}
