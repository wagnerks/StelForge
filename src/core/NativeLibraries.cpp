#include "NativeLibraries.h"

#include "FileSystem.h"

#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace SFE {
	void NativeLibraries::init() {
#ifdef _WIN32
		const auto dir = FileSystem::executableDir() / "ThirdParty";
		std::error_code ec;
		if (!std::filesystem::is_directory(dir, ec)) {
			return;
		}
		SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		AddDllDirectory(dir.wstring().c_str());
#endif
	}
}
