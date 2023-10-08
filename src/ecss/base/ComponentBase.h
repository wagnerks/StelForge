#pragma once

#include "../Types.h"
#include "../memory/ComponentsArray.h"

namespace ecss {
	class ComponentInterface {
		friend class Registry;
		friend class Memory::ComponentsArray;
	protected:
		ComponentInterface(const ComponentInterface& other) = default;
		ComponentInterface(ComponentInterface&& other) noexcept = default;
		ComponentInterface& operator=(const ComponentInterface& other) = default;
		ComponentInterface& operator=(ComponentInterface&& other) noexcept = default;

	public:
		virtual ComponentInterface* clone(void* adr) = 0;
		virtual ComponentInterface* move(void* adr) = 0;
		ComponentInterface() = default;
		virtual ~ComponentInterface() = default;

		EntityId getEntityId() const { return mOwnerId; };
	private:
		EntityId mOwnerId = INVALID_ID; //this variable will be set by components manager or component array after placing into entity sector
	};

	template<class T>
	class Component : public ComponentInterface {
	public:
		inline static const ECSType STATIC_COMPONENT_TYPE_ID = StaticTypeCounter<ComponentInterface>::get<T>();
		inline static uint8_t staticComponentSectorIdx = 0;

		ComponentInterface* clone(void* adr) override {
			return new (adr)T(*static_cast<T*>(this));
		}

		ComponentInterface* move(void* adr) override {
			return new (adr)T(std::move(*static_cast<T*>(this)));
		}
	};
}
