#include "SystemManager.h"
#include "core/Engine.h"

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

	void SystemManager::addTickSystem(System* system, float ticks) {
		if (!mStepTickSystems) {
			mTickSystemThreads.push_back(startTickSystem(system, ticks));
			return;
		}
		system->setTick(ticks);
		mSteppedTickSystems.push_back({ system });
	}

	std::thread SystemManager::startTickSystem(System* system, float ticks) {
		system->setTick(ticks);
		return std::thread([system] {
			float mLastFrame = 0.f;
			
			while (SystemManagerAlive) {
				const auto currentFrame = static_cast<float>(SFE::Engine::clockSeconds());
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

		for (auto& ticked : mSteppedTickSystems) {
			const float step = 1.f / ticked.system->getTicks();
			ticked.accumulated += dt;
			while (ticked.accumulated >= step) {
				ticked.system->update(step);
				ticked.accumulated -= step;
			}
		}

		for (const auto system : mMainThreadSystems) {
			system->update(dt);
		}

		for (auto system : mSystemsMap) {
			if (system) { // slots are type-indexed, so gaps are normal
				system->debugUpdate(dt);
			}
		}
	}
}