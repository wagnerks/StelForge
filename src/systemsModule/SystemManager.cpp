#include "SystemManager.h"

#include "systemsModule/SystemBase.h"
#include "debugModule/Benchmark.h"

using namespace ecss;

SystemManager::SystemManager() {
}

SystemManager::~SystemManager() {
	for (const auto system : mSystemsMap) {
		delete system;
	}
}

void SystemManager::startTickSystems() {
	SFE::ThreadPool::instance()->addTask([this]() {
		const float frameDt = 1.f / mTickRate;
		auto startTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> elapsed;

		while (SFE::ThreadPool::isAlive()) {
			for (auto system : mTickSystems) {
				system->update(elapsed.count());
			}

			auto endTime = std::chrono::high_resolution_clock::now();
			elapsed = endTime - startTime;
			startTime = endTime;
		}
	});
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
