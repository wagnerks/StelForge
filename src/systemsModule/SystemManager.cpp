#include "SystemManager.h"

#include "systemsModule/SystemBase.h"
#include "debugModule/Benchmark.h"

namespace ecss {

	bool SystemManagerAlive = true;
	SystemManager::~SystemManager() {
		SystemManagerAlive = false;
		for (auto& tickThread : mTickSystemThreads) {
			tickThread.join();
		}

		for (const auto system : mSystemsMap) {
			delete system;
		}
	}

	std::thread SystemManager::startTickSystem(System* system, float ticks) {
		system->setTick(ticks);
		return std::thread([system] {
			float mLastFrame = 0.f;
			
			while (SystemManagerAlive) {
				const auto currentFrame = static_cast<float>(glfwGetTime());
				const auto mDeltaTime = currentFrame - mLastFrame;
				mLastFrame = currentFrame;

				system->update(mDeltaTime);

				const auto tickDelta = 1.f / system->getTicks();
				if (mDeltaTime < tickDelta) {
					std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>((tickDelta - mDeltaTime) * 1000.f * 1000.f)));
				}
			}
		});
	}

	void SystemManager::update(float_t dt) {
		FUNCTION_BENCHMARK;

		for (const auto system : mMainThreadSystems) {
			system->update(dt);
		}

		for (auto system : mSystemsMap) {
			system->debugUpdate(dt);
		}
	}
}