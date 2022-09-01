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

void EntityInterface::addElement(EntityInterface* child) {
	if (auto childParent = child->getParent()) {
		childParent->removeElement(child);
	}

	child->setParent(this);
	child->getComponent<TransformComponent>()->setParentTransform(getComponent<TransformComponent>());
	getComponent<TransformComponent>()->addChildTransform(child->getComponent<TransformComponent>());

	mElements.emplace_back(child);
}

void EntityInterface::removeElement(std::string_view childId) {
	if (const auto child = getElement(childId)) {
		removeElement(child);
	}
}

void EntityInterface::removeElement(EntityInterface* child) {
	std::erase(mElements, child);
	child->setParent(nullptr);
	child->getComponent<TransformComponent>()->setParentTransform(nullptr);
	getComponent<TransformComponent>()->removeChildTransform(child->getComponent<TransformComponent>());
}

EntityInterface* EntityInterface::getParent() const {
	return mParent;
}

EntityInterface* EntityInterface::findElement(std::string_view elementId) {
	if (auto foundElement = getElement(elementId)) {
		return foundElement;
	}

	for (const auto element : mElements) {
		if (const auto foundElement = element->findElement(elementId)) {
			return foundElement;
		}
	}

	return nullptr;
}

std::vector<EntityInterface*> EntityInterface::getAllNodes() {
	std::vector<EntityInterface*> res;
	getAllNodesHelper(res);
	return res;
}

const std::vector<EntityInterface*>& EntityInterface::getElements() {
	return mElements;
}

EntityInterface* EntityInterface::getElement(std::string_view elementId) {
	auto it = std::ranges::find_if(mElements, [elementId](EntityInterface* child) {
		return elementId == child->getStringId();
	});
	if (it != mElements.end()) {
		return *it;
	}
	return nullptr;
}

std::string_view EntityInterface::getStringId() {
	return mStringId;
}

void EntityInterface::setStringId(std::string_view id) {
	mStringId = id;
}

void EntityInterface::getAllNodesHelper(std::vector<EntityInterface*>& res) {
	res.insert(res.end(), mElements.begin(), mElements.end());
	for (auto element : mElements) {
		element->getAllNodesHelper(res);
	}
}

void EntityInterface::setParent(EntityInterface* parentNode) {
	mParent = parentNode;
}

EntityInterface::~EntityInterface() {
	releaseComponents();

	for (const auto node : getElements()) {
		ECSHandler::entityManagerInstance()->destroyEntity(node->getEntityID());
		if (const auto comp = getComponent<TransformComponent>()){
			comp->setParentTransform(nullptr);
			getComponent<TransformComponent>()->removeChildTransform(comp);
		}
	}
}
