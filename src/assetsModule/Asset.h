#pragma once
#include <filesystem>
#include <string>

namespace AssetsModule {
	class Asset {
	public:
		virtual ~Asset() {}
		Asset() = default;

		std::string assetPath;
		// Whether assetPath names a file, and when that file was written as the asset was read from it
		// (AssetsManager::stamp).
		bool onDisk = false;
		std::filesystem::file_time_type written{};
	};
}
