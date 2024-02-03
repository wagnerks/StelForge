#pragma once
#include <cassert>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "Asset.h"
#include "modelModule/Model.h"
#include "memoryModule/GlobalMemoryUser.h"

namespace AssetsModule {

	inline static constexpr size_t ASSETS_MEMORY_BUFFER_SIZE = 256ll * 1024u * 1024u;//256MB

	class AssetsManager : public SFE::Singleton<AssetsManager>, SFE::MemoryModule::GlobalMemoryUser {
	public:
		AssetsManager();
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

			void* pAssetMem = mAssetsAllocator->allocate(sizeof(T), alignof(T));
			if (!pAssetMem) {
				assert(false);
				return nullptr;
			}

			asset = new(pAssetMem)T(std::forward<ARGS>(Args)...);

			auto id = mHasher(path);
			asset->assetId = id;
			mAssetsMap.insert({ id, asset });

			return asset;
		}

	private:
		SFE::MemoryModule::Allocator* mAssetsAllocator;
		std::unordered_map<size_t, Asset*> mAssetsMap;
		std::hash<std::string> mHasher;


	};

}

