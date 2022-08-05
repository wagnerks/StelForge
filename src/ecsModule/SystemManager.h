#pragma once
#include <unordered_map>

#include "SystemBase.h"
#include "memoryModule/LinearAllocator.h"
#include "memoryModule/MemoryChunkAllocator.h"

namespace ecsModule {
	class SystemInterface;

	class SystemManager : GameEngine::MemoryModule::GlobalMemoryUser {
		GameEngine::MemoryModule::LinearAllocator* mSystemAllocator;

		std::unordered_map<uint64_t, SystemInterface*> mSystemsMap;
		std::vector<std::vector<bool>> mSystemDependencyMatrix;

		std::vector<SystemInterface*> mWorkQueue;

	public:
		void update(float_t dt);
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

			const auto STID = T::STATIC_SYSTEM_TYPE_ID;

			void* pSystemMem = mSystemAllocator->allocate(sizeof(T), alignof(T));
			if (pSystemMem != nullptr) {
				system = new(pSystemMem)T(std::forward<ARGS>(systemArgs)...);
				mSystemsMap[STID] = system;
			}
			else {
				assert(false);
				return nullptr;
			}

			if (STID + 1 > mSystemDependencyMatrix.size()) {
				mSystemDependencyMatrix.resize(STID + 1);
				for (int i = 0; i < mSystemDependencyMatrix.size(); ++i)
					mSystemDependencyMatrix[i].resize(STID + 1);
			}

			mWorkQueue.push_back(system);

			return system;
		}

		template <class System_, class Dependency_>
		void addSystemDependency(System_ target, Dependency_ dependency) {
			const uint64_t TARGET_ID = target->GetStaticsize_t();
			const uint64_t DEPEND_ID = dependency->GetStaticsize_t();

			if (mSystemDependencyMatrix[TARGET_ID][DEPEND_ID] != true) {
				mSystemDependencyMatrix[TARGET_ID][DEPEND_ID] = true;
			}
		}

		template <class Target_, class Dependency_, class... Dependencies>
		void addSystemDependency(Target_ target, Dependency_ dependency, Dependencies&&... dependencies) {
			const uint64_t TARGET_ID = target->GetStaticsize_t();
			const uint64_t DEPEND_ID = dependency->GetStaticsize_t();

			if (mSystemDependencyMatrix[TARGET_ID][DEPEND_ID] != true) {
				mSystemDependencyMatrix[TARGET_ID][DEPEND_ID] = true;
			}

			addSystemDependency(target, std::forward<Dependencies>(dependencies)...);
		}
		
		template <class T>
		void enableSystem() {
			if (auto system = getSystem<T>()) {
				if (system->mEnabled) {
					return;
				}

				system->mEnabled = true;
			}
		}


		template <class T>
		void disableSystem() {
			if (auto system = getSystem<T>()) {
				if (!system->mEnabled) {
					return;
				}

				system->mEnabled = false;
			}
		}

		template <class T>
		void setSystemUpdateInterval(float_t interval_ms) {
			if (auto system = getSystem<T>()) {
				system->mUpdateInterval = interval_ms;
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


		std::vector<bool> GetSystemWorkState() const;


		void setSystemWorkState(const std::vector<bool>& mask);

		template <class... ActiveSystems>
		std::vector<bool> generateActiveSystemWorkState(ActiveSystems&&... activeSystems) {
			std::vector<bool> mask(mWorkQueue.size(), false);

			std::list<SystemInterface*> AS = {activeSystems...};
			for (auto s : AS) {
				for (int i = 0; i < mWorkQueue.size(); ++i) {
					if (mWorkQueue[i]->getStaticSystemTypeID() == s->getStaticSystemTypeID()) {
						mask[i] = true;
						break;
					}
				}
			}

			return mask;
		}
	};
}
