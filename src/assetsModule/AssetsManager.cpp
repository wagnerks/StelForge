#include "AssetsManager.h"


namespace AssetsModule {
	AssetsManager::~AssetsManager() {
		for (const auto& [id, asset] : mAssetsMap) {
			delete asset;
		}
	}
}
