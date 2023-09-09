#include "AssetsManager.h"

#include "memoryModule/LinearAllocator.h"

namespace AssetsModule {
	AssetsManager::AssetsManager(Engine::MemoryModule::MemoryManager* memoryManager) : GlobalMemoryUser(memoryManager) {
		auto size = ASSETS_MEMORY_BUFFER_SIZE;
		auto user = std::type_index(typeid(this)).hash_code();
		mAssetsAllocator = new Engine::MemoryModule::LinearAllocator(size, allocate(size, user));
	}

	AssetsManager::~AssetsManager() {
		for (const auto& mAsset : mAssetsMap) {
			mAsset.second->~Asset();
		}

		free(const_cast<void*>(mAssetsAllocator->getStartAddress())); //we allocate memory in global memory addresses, so we need to free it 
		delete mAssetsAllocator;
	}
}
