#include "AssetsManager.h"

#include "memoryModule/Allocators.h"

namespace AssetsModule {
	AssetsManager::AssetsManager() : GlobalMemoryUser(new SFE::MemoryModule::MemoryManager(ASSETS_MEMORY_BUFFER_SIZE)) {
		auto size = ASSETS_MEMORY_BUFFER_SIZE - 1;
		auto user = std::type_index(typeid(this)).hash_code();
		mAssetsAllocator = new SFE::MemoryModule::LinearAllocator();
		mAssetsAllocator->init(size, allocate(size, user));
	}

	AssetsManager::~AssetsManager() {
		for (const auto& mAsset : mAssetsMap) {
			mAsset.second->~Asset();
		}

		free(const_cast<void*>(mAssetsAllocator->getStartAddress())); //we allocate memory in global memory addresses, so we need to free it 
		delete mAssetsAllocator;

		delete mGlobalMemoryManager;
	}
}
