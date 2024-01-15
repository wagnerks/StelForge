#pragma once

#include "systemsModule/SystemBase.h"

namespace Engine {
	namespace MemoryModule {
		class ECSMemoryStack;
	}
}

namespace ecss {
	struct SystemsGraph {
		std::vector<System*> children;
	};

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
			system->mType = ecss::SystemTypeCounter::type<T>();
			mSystemsMap.push_back(system);

			return system;
		}

		template <class T, class... ARGS>
		void setSystemDependencies() {
			auto system = getSystem<T>();

			(system->template addDependency<ARGS>(getSystem<ARGS>()), ...);
		}
		
		template <class... ARGS>
		void addRootSystems() {
			(mRenderRoot.children.push_back(getSystem<ARGS>()), ...);
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
		void setUpdateInterval(float_t updateInterval) {
			if (auto system = getSystem<T>()) {
				system->mUpdateInterval = updateInterval;
			}
		}
		
	private:
		SystemsGraph mRenderRoot;

		std::vector<System*> mSystemsMap;
	};
}
