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
		const std::chrono::duration<float> frameDuration(frameDt);

		while (SFE::ThreadPool::isAlive()) {
			auto startTime = std::chrono::high_resolution_clock::now();
			for (auto system : mTickSystems) {
				system->update(frameDt);
			}

			auto endTime = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(endTime - startTime);
			mTickDt = elapsed.count();
			auto sleepTime = frameDuration - elapsed;
			if (sleepTime.count() <= 0.f) {
				continue;
			}
			std::this_thread::sleep_for(sleepTime);
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
