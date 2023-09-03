#pragma once
#include <typeindex>

#include "memoryModule/MemoryChunkAllocator.h"


class ContainerInterface {
public:
	virtual ~ContainerInterface() = default;
	virtual const char* getComponentContainerTypeName() const = 0;
	virtual void destroyElement(void* object) = 0;
};

template <class interfaceType, size_t Capacity = 1024>
class Container : public Engine::MemoryModule::MemoryChunkAllocator<interfaceType, Capacity>, public ContainerInterface {
public:
	Container(size_t userId, Engine::MemoryModule::MemoryManager* memoryManager) : Engine::MemoryModule::MemoryChunkAllocator<interfaceType, Capacity>(userId, memoryManager) {}

	const char* getComponentContainerTypeName() const override;
	void destroyElement(void* object) override;
};

template <class interfaceType, size_t Capacity>
const char* Container<interfaceType, Capacity>::getComponentContainerTypeName() const {
	static const char* COMPONENT_TYPE_NAME{std::type_index(typeid(interfaceType)).name()};
	return COMPONENT_TYPE_NAME;
}

template <class interfaceType, size_t Capacity>
void Container<interfaceType, Capacity>::destroyElement(void* object) {
	static_cast<interfaceType*>(object)->~interfaceType();
	this->destroyObject(object);
}
