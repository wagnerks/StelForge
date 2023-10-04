#pragma once

#include "../Types.h"
#include "../memory/ComponentsArray.h"

namespace ECS {
	class ComponentInterface {
		friend class ComponentManager;
		friend class Memory::ComponentsArray;

		ComponentInterface(const ComponentInterface& other) = delete;
		ComponentInterface(ComponentInterface&& other) noexcept = delete;
		ComponentInterface& operator=(const ComponentInterface& other) = delete;
		ComponentInterface& operator=(ComponentInterface&& other) noexcept = delete;
	public:
		ComponentInterface() = default;
		virtual ~ComponentInterface() = default;

		EntityId getOwnerId() const { return mOwnerId; };
	private:
		EntityId mOwnerId = INVALID_ID; //this variable will be set by components manager or component array after placing into entity sector
	};

	template<class T>
	class Component : public ComponentInterface {
	public:
		inline static const ECSType STATIC_COMPONENT_TYPE_ID = StaticTypeCounter<ComponentInterface>::get<T>();
	};
}
