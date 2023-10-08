#pragma once
#include <cassert>
#include <mutex>
#include <unordered_map>

#include "memoryModule/Allocators.h"

namespace Engine {
	namespace MemoryModule {
		class ECSMemoryStack;
	}
}

namespace ecss {
	class EntityComponentSystem;
	class SystemInterface;

	class SystemManager final {
		SystemManager(const SystemManager&) = delete;
		SystemManager& operator=(SystemManager&) = delete;
	public:
		SystemManager();
		~SystemManager();

		void update(float_t dt);
		void sortWorkQueue();

		template <class T>
		T* getSystem() {
			auto it = mSystemsMap.find(T::STATIC_SYSTEM_TYPE_ID);
			if (it != mSystemsMap.end() && it->second) {
				return static_cast<T*>(it->second);
			}

			return nullptr;
		}

		template <class T, class... ARGS>
		T* addSystem(ARGS&&... systemArgs) {
			auto system = getSystem<T>();
			if (system) {
				return system;
			}

			void* pSystemMem = mSystemAllocator.allocate(sizeof(T), alignof(T));
			if (!pSystemMem) {
				assert(false);
				return nullptr;
			}

			system = new(pSystemMem)T(std::forward<ARGS>(systemArgs)...);
			mSystemsMap[T::STATIC_SYSTEM_TYPE_ID] = system;

			mWorkQueue.push_back(system);
			sortWorkQueue();

			return system;
		}

		template <class T>
		void setSystemEnabled(bool enabled) {
			if (auto system = getSystem<T>()) {
				if (system->mEnabled == enabled) {
					return;
				}

				system->mEnabled = enabled;
			}
		}

		template <class T>
		void setSystemUpdateInterval(float_t updateInterval) {
			if (auto system = getSystem<T>()) {
				system->mUpdateInterval = updateInterval;
			}
		}

		template <class T>
		void setSystemPriority(uint16_t newPriority) {
			if (auto system = getSystem<T>()) {
				if (system->mPriority == newPriority) {
					return;
				}

				system->mPriority = newPriority;

				sortWorkQueue();
			}
		}

		std::mutex systemsMutex;
		std::condition_variable systemsLock;
		std::atomic_bool updating = false;
	private:
		Engine::MemoryModule::LinearAllocator mSystemAllocator;

		std::unordered_map<uint64_t, SystemInterface*> mSystemsMap;
		std::vector<SystemInterface*> mWorkQueue;

		Engine::MemoryModule::ECSMemoryStack* mMemoryManager = nullptr;
	};
}
