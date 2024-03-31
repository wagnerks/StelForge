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
		
		template <class T>
		T* getSystem() {
			if (mSystemsMap.size() > SystemTypeCounter::type<T>()) {
				return static_cast<T*>(mSystemsMap[SystemTypeCounter::type<T>()]);
			}

			return nullptr;
		}

		template <class T, class... ARGS>
		T* createSystem(ARGS&&... systemArgs) {
			auto system = getSystem<T>();
			if (system) {
				return system;
			}

			system = new T(std::forward<ARGS>(systemArgs)...);
			ecss::SystemTypeCounter::type<T>();
			mSystemsMap.push_back(system);

			return system;
		}

		
		template <class... ARGS>
		void addRootSystems() {
			(mMainThreadSystems.push_back(createSystem<ARGS>()), ...);
		}

		template <class... ARGS>
		void addTickSystems(float ticks = 32) {
			(mTickSystemThreads.push_back(startTickSystem(createSystem<ARGS>(), ticks)), ...);
		}

	private:
		std::thread startTickSystem(System* system, float ticks = 32);

		std::vector<System*> mSystemsMap;

		std::vector<System*> mMainThreadSystems;

		std::vector<std::thread> mTickSystemThreads;
	};
}
