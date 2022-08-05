#pragma once
#include "helper.h"

namespace ecsModule {
	class ComponentInterface {
	public:
		ComponentInterface(const ComponentInterface& other) = delete;
		ComponentInterface(ComponentInterface&& other) noexcept = delete;
		ComponentInterface& operator=(const ComponentInterface& other) = delete;
		ComponentInterface& operator=(ComponentInterface&& other) noexcept = delete;

		ComponentInterface() = default;
		virtual ~ComponentInterface() = default;
		
		size_t getOwnerId() const;
		size_t getComponentId() const;
		void setOwnerId(size_t id);
		void setId(size_t id);
	private:
		size_t mOwnerId = INVALID_ID;
		size_t mComponentID = INVALID_ID;
	};

	template<class T>
	class Component: public ComponentInterface {
	public:
		inline static const size_t STATIC_COMPONENT_TYPE_ID = FamilySize<ComponentInterface>::Get<T>();
	};
}
