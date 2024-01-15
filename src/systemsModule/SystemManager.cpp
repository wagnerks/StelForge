#include "SystemManager.h"

#include "systemsModule/SystemBase.h"
#include "debugModule/Benchmark.h"

using namespace ecss;

SystemManager::~SystemManager() {
	for (const auto system : mSystemsMap) {
		delete system;
	}
}

void SystemManager::update(float_t dt) {
	FUNCTION_BENCHMARK;

	for (const auto system : mRenderRoot.children) {
		if (system->mEnabled && (system->mTimeFromLastUpdate += dt) >= system->mUpdateInterval) {
			system->mTimeFromLastUpdate = 0.f;
			system->update(dt);
		}
	}

	for (auto system : mSystemsMap) {
		if (system->mEnabled) {
			system->debugUpdate(dt);
		}
	}
}