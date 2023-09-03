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

void EntityInterface::operator delete(void* ptr) {
	ECSHandler::entityManagerInstance()->destroyEntity(static_cast<EntityInterface*>(ptr)->getEntityID());
}

EntityInterface::~EntityInterface() {
	releaseComponents();
}

EntityInterface::EntityInterface(size_t entityID) : mId(entityID) {
	mOnElementRemove = [this](EntityInterface* entity) {
		const auto childComponent = entity->getComponent<TransformComponent>();
		const auto currentCmp = getComponent<TransformComponent>();

		if (childComponent) {
			childComponent->setParentTransform(nullptr);
		}

		if (currentCmp) {
			currentCmp->removeChildTransform(childComponent);
		}
	};

	mOnElementAdd = [this](EntityInterface* entity) {
		const auto childComponent = entity->getComponent<TransformComponent>();
		const auto currentCmp = getComponent<TransformComponent>();

		if (childComponent) {
			childComponent->setParentTransform(currentCmp);
		}

		if (currentCmp) {
			currentCmp->addChildTransform(childComponent);
		}
	};
}


