#pragma once
#include "Singleton.h"
#include "ecss/Registry.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/systems/RenderSystem.h"

class ECSHandler final : public SFE::Singleton<ECSHandler> {
public:
	template<class SystemType>
	inline static SystemType* getSystem() {
		return systemManager().getSystem<SystemType>();
	}

	inline static ecss::SystemManager& systemManager() {
		return instance()->mSystemManager;
	}

	inline static ecss::Registry& registry() {
		return instance()->mRegistry;
	}

	template<typename CompType, class ...Args>
	inline static CompType* addComponent(ecss::EntityId entity, Args&&... args) {
		auto comp = instance()->mRegistry.addComponent<CompType>(entity, std::forward<Args>(args)...);
		if constexpr (std::is_same<CompType, MaterialComponent>()) {
			if (auto renderSys = getSystem<SFE::SystemsModule::RenderSystem>()) {
				renderSys->markDirty<CompType>(entity);
			}
		}
		else if constexpr (std::is_same<CompType, MaterialComponent>()) {

		}
		

		return comp;
	}

	template<typename CompType>
	inline static void removeComponent(ecss::EntityId entity) {
		instance()->mRegistry.removeComponent<CompType>(entity);
		if (auto renderSys = getSystem<SFE::SystemsModule::RenderSystem>()) {
			renderSys->markRemoved<CompType>(entity);
		}
	}

	template<typename CompType>
	inline static void removeComponent(std::vector<ecss::EntityId>& entities) {
		instance()->mRegistry.removeComponent<CompType>(entities);
		if (!entities.empty()) {
			if (auto renderSys = getSystem<SFE::SystemsModule::RenderSystem>()) {
				for (auto entity : entities) {
					renderSys->markRemoved<CompType>(entity);
				}
			}
		}
	}
	
	inline static ecss::Registry& drawRegistry(uint8_t index = 0) {
		return instance()->mDrawRegistry[index];
	}
	
	void initSystems();

private:
	ecss::SystemManager mSystemManager;
	ecss::Registry mRegistry;
	std::array<ecss::Registry, 2> mDrawRegistry;
};
