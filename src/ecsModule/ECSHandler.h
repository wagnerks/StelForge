#pragma once

namespace GameEngine {
	namespace MemoryModule {
		class MemoryManager;
	}
}

namespace ecsModule {
	class ComponentManager;
	class SystemManager;
	class EntityManager;

	class ECSHandler {
	public:
		static SystemManager* systemManagerInstance();
		static EntityManager* entityManagerInstance();
		static ComponentManager* componentManagerInstance();

		static ECSHandler* getInstance();

		static void terminate();

		void init();
		void initSystems();
		~ECSHandler();
	private:
		inline static ECSHandler* instance = nullptr;
		GameEngine::MemoryModule::MemoryManager* memoryManager = nullptr;
		SystemManager* systemManager = nullptr;
		EntityManager* entityManager = nullptr;
		ComponentManager* componentManager = nullptr;
	};
}
