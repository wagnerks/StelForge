#pragma once
#include <cassert>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "Asset.h"

#include "memoryModule/MemoryChunkAllocator.h"

namespace AssetsModule {

	inline static constexpr size_t ASSETS_MEMORY_BUFFER_SIZE = 8ll * 1024u * 1024u;//256MB

	class AssetsManager : Engine::MemoryModule::GlobalMemoryUser {
	public:
		AssetsManager(Engine::MemoryModule::MemoryManager* memoryManager);

		~AssetsManager() override;

		template <class T>
		T* getAsset(const std::string& path) {
			if (const auto it = mAssetsMap.find(mHasher(path)); it != mAssetsMap.end()) {
				return static_cast<T*>(it->second);
			}

			return nullptr;
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
			mAssetsMap.insert({ mHasher(path) , asset });

			return asset;
		}

	private:
		Engine::MemoryModule::Allocator* mAssetsAllocator;
		std::unordered_map<size_t, Asset*> mAssetsMap;
		std::hash<std::string> mHasher;
	};

}

