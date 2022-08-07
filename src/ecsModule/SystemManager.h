#pragma once
#include <unordered_map>

#include "SystemBase.h"
#include "memoryModule/LinearAllocator.h"
#include "memoryModule/MemoryChunkAllocator.h"

namespace ecsModule {
	class SystemInterface;

	class SystemManager : GameEngine::MemoryModule::GlobalMemoryUser {
	public:
		void update(float_t dt) const;
		SystemManager(const SystemManager&) = delete;
		SystemManager& operator=(SystemManager&) = delete;

		SystemManager(GameEngine::MemoryModule::MemoryManager* memoryManager);
		~SystemManager() override;

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

			void* pSystemMem = mSystemAllocator->allocate(sizeof(T), alignof(T));
			if (!pSystemMem) {
				assert(false);
				return nullptr;
			}

			system = new(pSystemMem)T(std::forward<ARGS>(systemArgs)...);
			mSystemsMap[T::STATIC_SYSTEM_TYPE_ID] = system;

			mWorkQueue.push_back(system);

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
		void setSystemPriority(size_t newPriority) {
			if (auto system = getSystem<T>()) {
				if (system->mPriority == newPriority) {
					return;
				}

				system->mPriority = newPriority;
			}
		}
	private:
		GameEngine::MemoryModule::LinearAllocator* mSystemAllocator;

		std::unordered_map<uint64_t, SystemInterface*> mSystemsMap;
		std::vector<SystemInterface*> mWorkQueue;
	};
}
