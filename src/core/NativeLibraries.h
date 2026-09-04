#pragma once

namespace SFE {
	// Registers <exe>/ThirdParty with the Windows PE loader (CMake stages native DLLs there).
	// Call before the first use of those libraries. Delay-load on the exe makes that possible.
	struct NativeLibraries {
		static void init();
	};
}
