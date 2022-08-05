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

void SystemManager::update(float_t dt) {
	for (SystemInterface* system : mWorkQueue) {
		system->mTimeSinceLastUpdate += dt;

		system->mNeedsUpdate = (system->mUpdateInterval < 0.0f) || ((system->mUpdateInterval > 0.0f) && (system->
			mTimeSinceLastUpdate > system->mUpdateInterval));

		if (system->mEnabled == true && system->mNeedsUpdate == true) {
			system->preUpdate(dt);
		}
	}

	for (SystemInterface* system : mWorkQueue) {
		if (system->mEnabled == true && system->mNeedsUpdate == true) {
			system->update(dt);

			system->mTimeSinceLastUpdate = 0.0f;
		}
	}

	for (SystemInterface* system : mWorkQueue) {
		if (system->mEnabled == true && system->mNeedsUpdate == true) {
			system->postUpdate(dt);
		}
	}
}

std::vector<bool> SystemManager::GetSystemWorkState() const {
	std::vector<bool> mask(mWorkQueue.size());

	for (auto i = 0u; i < mWorkQueue.size(); ++i) {
		mask[i] = mWorkQueue[i]->mEnabled;
	}

	return mask;
}

void SystemManager::setSystemWorkState(const std::vector<bool>& mask) {
	assert(mask.size() == mWorkQueue.size() && "Provided mask does not match size of current system array.");

	for (int i = 0; i < mWorkQueue.size(); ++i) {
		mWorkQueue[i]->mEnabled = mask[i];
	}
}
