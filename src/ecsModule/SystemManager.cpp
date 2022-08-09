#include "SystemManager.h"

#include <functional>
#include <ranges>

#include "helper.h"
#include "SystemInterface.h"

using namespace ecsModule;

SystemManager::SystemManager(GameEngine::MemoryModule::MemoryManager* memoryManager) : GlobalMemoryUser(memoryManager) {
	mSystemAllocator = new GameEngine::MemoryModule::LinearAllocator(ecsModule::ECS_SYSTEM_MEMORY_BUFFER_SIZE, allocate(ecsModule::ECS_SYSTEM_MEMORY_BUFFER_SIZE, 3));
}

SystemManager::~SystemManager() {
	for (auto& it : std::ranges::reverse_view(mWorkQueue)) {
		it->~SystemInterface();
	}

	globalMemoryManager->free(const_cast<void*>(mSystemAllocator->getStartAddress())); //we allocate memory in global memory addresses, so we need to free it 
	delete mSystemAllocator;
}

void SystemManager::sortWorkQueue() {
	std::ranges::sort(mWorkQueue, [](const SystemInterface* a, const SystemInterface* b) {
		return a->mPriority > b->mPriority;
	});
}

void SystemManager::update(float_t dt) const {
	for (SystemInterface* system : mWorkQueue) {
		system->mTimeSinceLastUpdate += dt;
		if (!system->mEnabled) {
			continue;
		}

		if (system->mTimeSinceLastUpdate > system->mUpdateInterval || system->mUpdateInterval == 0.f) {
			system->preUpdate(dt);
		}
	}

	for (SystemInterface* system : mWorkQueue) {
		if (!system->mEnabled) {
			continue;
		}

		if (system->mTimeSinceLastUpdate > system->mUpdateInterval || system->mUpdateInterval == 0.f) {
			system->update(dt);
		}
	}

	for (SystemInterface* system : mWorkQueue) {
		if (!system->mEnabled) {
			continue;
		}

		if (system->mTimeSinceLastUpdate > system->mUpdateInterval || system->mUpdateInterval == 0.f) {
			system->postUpdate(dt);

			system->mTimeSinceLastUpdate = 0.0f;
		}
	}
}
