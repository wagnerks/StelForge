#pragma once
#include "Singleton.h"
#include "ecss/Registry.h"
#include "systemsModule/SystemManager.h"


class ECSHandler final : public Engine::Singleton<ECSHandler> {
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

	void initSystems();

private:
	ecss::SystemManager mSystemManager;
	ecss::Registry mRegistry;
};