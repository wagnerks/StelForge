#pragma once
#include <cassert>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "Asset.h"
#include "containersModule/Singleton.h"
#include "modelModule/Model.h"

namespace AssetsModule {

	class AssetsManager : public SFE::Singleton<AssetsManager> {
	public:
		AssetsManager() = default;
		~AssetsManager() override;

		template <class T>
		T* getAsset(size_t hashId) {
			if (const auto it = mAssetsMap.find(hashId); it != mAssetsMap.end()) {
				return static_cast<T*>(it->second);
			}

			return nullptr;
		}

		template <class T>
		T* getAsset(const std::string& path) {
			return getAsset<T>(mHasher(path));
		}

		template <class T, class... ARGS>
		T* createAsset(const std::string& path, ARGS&&... Args) {
			auto* asset = getAsset<T>(path);
			if (asset) {
				return asset;
			}

			asset = new T(std::forward<ARGS>(Args)...);

			auto id = mHasher(path);
			asset->assetId = id;
			asset->assetPath = path;
			mAssetsMap.insert({ id, asset });

			return asset;
		}

	private:
		std::unordered_map<size_t, Asset*> mAssetsMap;
		std::hash<std::string> mHasher;
	};
}

