#include "Node.h"

#include "componentsModule/TransformComponent.h"
#include "ecsModule/EntityManager.h"

using namespace GameEngine::NodeModule;

Node::Node(size_t entID, std::string_view id) : Entity<Node>(entID), id(id) {
	addComponent<TransformComponent>();
}
Node::~Node() {
	for (auto node : getElements()) {
		ecsModule::ECSHandler::entityManagerInstance()->destroyEntity(node->getEntityID());
		if (auto comp = ecsModule::ECSHandler::componentManagerInstance()->getComponent<TransformComponent>(node->getEntityID())) {
			comp->setParentTransform(nullptr);
			getComponent<TransformComponent>()->removeChildTransform(comp);
		}
		
	}
}

void Node::addElement(Node* child) {
	if (auto childParent = child->getParent()) {
		childParent->removeElement(child);
	}

	child->setParent(this);
	child->getComponent<TransformComponent>()->setParentTransform(getComponent<TransformComponent>());
	getComponent<TransformComponent>()->addChildTransform(child->getComponent<TransformComponent>());

	elements.emplace_back(child);
}

void Node::removeElement(std::string_view childId) {
	if (const auto child = getElement(childId)) {
		removeElement(child);
	}
}

void Node::removeElement(Node* child) {
	std::erase(elements, child);
	child->setParent(nullptr);
	child->getComponent<TransformComponent>()->setParentTransform(nullptr);
	getComponent<TransformComponent>()->removeChildTransform(child->getComponent<TransformComponent>());
}

Node* Node::getParent() const {
	return parent;

}

Node* Node::findElement(std::string_view elementId) {
	if (auto foundElement = getElement(elementId)) {
		return foundElement;
	}

	for (const auto element : elements) {
		if (const auto foundElement = element->findElement(elementId)) {
			return foundElement;
		}
	}

	return nullptr;
}

std::vector<Node*> Node::getAllNodes() {
	std::vector<Node*> res;
	getAllNodesHelper(res);
	return res;
}

const std::vector<Node*>& Node::getElements() {
	return elements;
}

Node* Node::getElement(std::string_view elementId) {
	auto it = std::ranges::find_if(elements, [elementId](Node* child) {
		return elementId == child->getId();
	});
	if (it != elements.end()) {
		return *it;
	}
	return nullptr;
}

std::string_view Node::getId() {
	return id;
}

void Node::setParent(Node* parentNode) {
	parent = parentNode;
}


void Node::getAllNodesHelper(std::vector<Node*>& res) {
	res.insert(res.end(), elements.begin(), elements.end());
	for (auto element : elements) {
		element->getAllNodesHelper(res);
	}
}
