#include "EntityBase.h"

#include "EntityManager.h"
#include "componentsModule/TransformComponent.h"

using namespace ecsModule;

size_t EntityInterface::getEntityID() const {
	return mId;
}

void EntityInterface::setId(size_t id) {
	mId = id;
}

void EntityInterface::operator delete(void* ptr){
	ECSHandler::entityManagerInstance()->destroyEntity(static_cast<EntityInterface*>(ptr)->getEntityID());
}

EntityInterface::~EntityInterface() {
	releaseComponents();

	for (const auto node : getElements()) {
		if (const auto comp = node->getComponent<TransformComponent>()){
			comp->setParentTransform(nullptr);
			//getComponent<TransformComponent>()->removeChildTransform(comp);
		}
	}
}
