#pragma once
#include <glm.hpp>
#include <map>
#include <typeindex>


namespace GameEngine::ComponentsModule {
	class Component;

	class ComponentHolder {
	public:
		~ComponentHolder();
		void updateComponents() const;

	    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
	    T* addComponent() {
			return static_cast<T*>(components.insert(std::make_pair(std::type_index(typeid(T)), new T(this)))->second);
	    }

	    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
	    T* getComponent() {
	        auto it = components.find(std::type_index(typeid(T)));
	        if (it == components.end()) {
	        	return addComponent<T>();
			}
	        return static_cast<T*>(it->second);
	    }

	private:
		std::multimap<std::type_index, Component*> components;
	};

	
}

