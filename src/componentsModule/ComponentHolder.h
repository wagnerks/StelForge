#pragma once
#include <glm.hpp>
#include <map>
#include <typeindex>
#include <unordered_map>


namespace GameEngine::ComponentsModule {
	class Component;

	class ComponentHolder {
	public:
		virtual ~ComponentHolder();
		void updateComponents() const;

	    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
	    T* addComponent() {
			return static_cast<T*>(components.insert(std::make_pair(std::type_index(typeid(T)), new T(this))).first->second);
	    }

	    template<typename T, typename = std::enable_if_t<std::is_base_of_v<Component, T>>>
	    T* getComponent(bool add = true) {
	        auto it = components.find(std::type_index(typeid(T)));
	        if (it == components.end()) {
				if (add) {
					return addComponent<T>();
				}
	        	return nullptr;
			}
	        return static_cast<T*>(it->second);
	    }

	private:
		std::unordered_map<std::type_index, Component*> components;
	};

	
}

