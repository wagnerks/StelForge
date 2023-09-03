#pragma once
#include "core/Singleton.h"

namespace Engine {
	namespace MemoryModule {
		class MemoryManager;
	}
}

namespace ecsModule {
	class ComponentManager;
	class SystemManager;
	class EntityManager;

	class ECSHandler : public Engine::Singleton<ECSHandler> {
		friend Singleton;
	public:
		static SystemManager* systemManagerInstance();
		static EntityManager* entityManagerInstance();
		static ComponentManager* componentManagerInstance();

		void init() override;
		void initSystems();
		~ECSHandler();
	private:
		Engine::MemoryModule::MemoryManager* memoryManager = nullptr;
		SystemManager* systemManager = nullptr;
		EntityManager* entityManager = nullptr;
		ComponentManager* componentManager = nullptr;
	};
}
