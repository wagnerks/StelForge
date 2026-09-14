#pragma once

#include "systemsModule/SystemBase.h"

namespace ecss {
	class SystemManager final {
		SystemManager(const SystemManager&) = delete;
		SystemManager& operator=(SystemManager&) = delete;
	public:
		SystemManager() = default;
		~SystemManager();
		
		void update(float_t dt);
		
		// mSystemsMap is indexed BY type id, never by creation order. SystemTypeCounter hands
		// out ids lazily on first touch, so a getSystem<X>() from inside another system's
		// constructor claims X's id early and the two orders diverge for good -- which used to
		// make createSystem<T>() hand back an unrelated system and skip constructing T.
		template <class T>
		T* getSystem() {
			const size_t index = SystemTypeCounter::type<T>();
			return index < mSystemsMap.size() ? static_cast<T*>(mSystemsMap[index]) : nullptr;
		}

		template <class T, class... ARGS>
		T* createSystem(ARGS&&... systemArgs) {
			const size_t index = SystemTypeCounter::type<T>();
			if (index < mSystemsMap.size() && mSystemsMap[index]) {
				return static_cast<T*>(mSystemsMap[index]);
			}

			auto system = new T(std::forward<ARGS>(systemArgs)...);
			system->bindWorkers();

			// The constructor above may have created further systems, so claim the slot only
			// once it has returned.
			if (mSystemsMap.size() <= index) {
				mSystemsMap.resize(index + 1, nullptr);
			}
			mSystemsMap[index] = system;

			return system;
		}

		
		template <class... ARGS>
		void addRootSystems() {
			(mMainThreadSystems.push_back(createSystem<ARGS>()), ...);
		}

		// Tick systems run at their own rate: on a thread each by default, or -- once this is set, before they
		// are added -- stepped from update() by the frame time, so a fixed frame step makes them deterministic.
		void setStepTickSystems(bool stepped) { mStepTickSystems = stepped; }

		template <class... ARGS>
		void addTickSystems(float ticks = 32) {
			(addTickSystem(createSystem<ARGS>(), ticks), ...);
		}

	private:
		struct SteppedTickSystem {
			System* system = nullptr;
			float accumulated = 0.f;
		};

		void addTickSystem(System* system, float ticks);
		std::thread startTickSystem(System* system, float ticks = 32);

		std::vector<System*> mSystemsMap;

		std::vector<System*> mMainThreadSystems;

		std::vector<std::thread> mTickSystemThreads;
		std::vector<SteppedTickSystem> mSteppedTickSystems;
		bool mStepTickSystems = false;
	};
}
