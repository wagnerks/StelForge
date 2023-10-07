#pragma once

namespace ECS {
	namespace Memory {
		class ECSMemoryStack;
	}

	class ComponentManager;
	class SystemManager;
	class EntityManager;

	class EntityComponentSystem {
		EntityComponentSystem(const EntityComponentSystem& other) = delete;
		EntityComponentSystem(EntityComponentSystem&& other) noexcept = delete;
		EntityComponentSystem& operator=(const EntityComponentSystem& other) = delete;
		EntityComponentSystem& operator=(EntityComponentSystem&& other) noexcept = delete;
	public:

		EntityComponentSystem();
		~EntityComponentSystem();

		SystemManager* getSystemManager() const;
		EntityManager* getEntityManager() const;
		ComponentManager* getComponentManager() const;
	private:
		

		SystemManager* mSystemManager = nullptr;
		EntityManager* mEntityManager = nullptr;
		ComponentManager* mComponentManager = nullptr;
	};
}
