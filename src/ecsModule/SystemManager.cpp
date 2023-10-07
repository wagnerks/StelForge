#include "SystemManager.h"

#include <algorithm>

#include "base/SystemBase.h"
#include "memory/ECSMemoryStack.h"

using namespace ECS;

SystemManager::SystemManager() {
	const auto systemsSize = StaticTypeCounter<SystemInterface>::getSize();
	mMemoryManager = new Memory::ECSMemoryStack(systemsSize);

	mSystemAllocator.init(systemsSize, mMemoryManager->allocate(systemsSize));
}

SystemManager::~SystemManager() {
	for (const auto system : mWorkQueue) {
		system->~SystemInterface();
	}

	delete mMemoryManager;
}

void SystemManager::sortWorkQueue() {
	std::ranges::sort(mWorkQueue, [](const SystemInterface* a, const SystemInterface* b) {
		return a->mPriority > b->mPriority;
	});
}

void SystemManager::update(float_t dt) {
	systemsMutex.lock();
	updating = true;
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
	updating = false;

	systemsLock.notify_all();
	systemsMutex.unlock();
}
