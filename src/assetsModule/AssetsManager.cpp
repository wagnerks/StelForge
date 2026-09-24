#include "AssetsManager.h"

#include <filesystem>

#include "core/FileSystem.h"


namespace AssetsModule {
	AssetsManager::~AssetsManager() {
		for (const auto& [id, asset] : mAssets) {
			delete asset;
		}
		for (auto* asset : mReplaced) {
			delete asset;
		}
	}

	std::string AssetsManager::key(const std::string& path) {
		return std::filesystem::path(path).lexically_normal().generic_string();
	}

	void AssetsManager::stamp(Asset& asset) {
		std::error_code error;
		const auto written = std::filesystem::last_write_time(SFE::FileSystem::resolve(asset.assetPath), error);
		asset.onDisk = !error;
		asset.written = error ? std::filesystem::file_time_type{} : written;
	}

	bool AssetsManager::changedOnDisk(const Asset& asset) {
		if (!asset.onDisk) {
			return false;
		}
		std::error_code error;
		const auto written = std::filesystem::last_write_time(SFE::FileSystem::resolve(asset.assetPath), error);
		return !error && written != asset.written;
	}
}
