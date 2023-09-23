#pragma once

namespace Engine {
	namespace MemoryModule {
		class MemoryManager;
	}
}

namespace ecsModule {
	class ComponentManager;
	class SystemManager;
	class EntityManager;

	class EntityComponentSystem {
	public:
		EntityComponentSystem(Engine::MemoryModule::MemoryManager* memoryManager);
		~EntityComponentSystem();

		SystemManager* getSystemManager() const;
		EntityManager* getEntityManager() const;
		ComponentManager* getComponentManager() const;
	private:
		Engine::MemoryModule::MemoryManager* mMemoryManager = nullptr;

		SystemManager* mSystemManager = nullptr;
		EntityManager* mEntityManager = nullptr;
		ComponentManager* mComponentManager = nullptr;
	};
}
