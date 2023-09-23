#pragma once
#include "Singleton.h"

namespace Engine {
	namespace MemoryModule {
		class MemoryManager;
	}
}

namespace ecsModule {
	class EntityComponentSystem;
	class ComponentManager;
	class EntityManager;
	class SystemManager;
}

class ECSHandler : public Engine::Singleton<ECSHandler> {
public:
	ECSHandler();
	static ecsModule::SystemManager* systemManagerInstance();
	static ecsModule::EntityManager* entityManagerInstance();
	static ecsModule::ComponentManager* componentManagerInstance();
	void initSystems();

	~ECSHandler() override;
private:
	ecsModule::EntityComponentSystem* ECS = nullptr;
	Engine::MemoryModule::MemoryManager* ECSMemoryManager = nullptr;
};
