#pragma once
#include "Singleton.h"

namespace ecss {
	class Registry;
	class SystemManager;
}

class ECSHandler : public Engine::Singleton<ECSHandler> {
public:
	ECSHandler();;
	static ecss::SystemManager* systemManager();
	static ecss::Registry* registry();
	void initSystems();
private:
	ecss::SystemManager* mSystemManager = nullptr;
	ecss::Registry* mRegistry = nullptr;
};
