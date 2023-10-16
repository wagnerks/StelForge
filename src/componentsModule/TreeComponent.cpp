#include "TreeComponent.h"

#include "core/ECSHandler.h"

namespace Engine::ComponentsModule {
	void TreeComponent::setParent(ecss::EntityId id) {
		if (mParentEntity == id) {
			return;
		}

		mParentEntity = id;		
	}

	ecss::EntityId TreeComponent::getParent() const {
		return mParentEntity;
	}

	void TreeComponent::addChildEntity(ecss::EntityId id) {
		if (std::find(mChildrenEntities.begin(), mChildrenEntities.end(), id) == mChildrenEntities.end()) {
			mChildrenEntities.push_back(id);
			if (auto tree = ECSHandler::registry()->getComponent<TreeComponent>(id)) {
				tree->setParent(getEntityId());;
			}
		}
	}

	void TreeComponent::removeChildEntity(ecss::EntityId id) {
		std::erase(mChildrenEntities, id);
		if (auto tree = ECSHandler::registry()->getComponent<TreeComponent>(id)) {
			tree->setParent(ecss::INVALID_ID);
		}
	}

	std::vector<ecss::EntityId> TreeComponent::getAllNodes() {
		std::vector<ecss::EntityId> allNodes;
		getAllNodesHelper(allNodes);
		return allNodes;
	}

	void TreeComponent::getAllNodesHelper(std::vector<ecss::EntityId>& res) {
		res.insert(res.end(), mChildrenEntities.begin(), mChildrenEntities.end());
		for (auto element : mChildrenEntities) {
			if (auto childNode = ECSHandler::registry()->getComponent<TreeComponent>(element)) {
				childNode->getAllNodesHelper(res);
			}
		}
	}

	const std::vector<ecss::EntityId>& TreeComponent::getChildren() {
		return mChildrenEntities;
	}

	TreeComponent::~TreeComponent() {
		if (const auto parent = ECSHandler::registry()->getComponent<TreeComponent>(mParentEntity)) {
			parent->removeChildEntity(getEntityId());
		}

		for (const auto child : mChildrenEntities) {
			if (auto childNode = ECSHandler::registry()->getComponent<TreeComponent>(child)) {
				childNode->setParent(ecss::INVALID_ID);
			}
		}
	}
}
