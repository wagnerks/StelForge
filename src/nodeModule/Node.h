#pragma once
#include <string_view>
#include <vector>

#include "ecsModule/EntityBase.h"


namespace GameEngine::NodeModule {

	class Node : public ecsModule::Entity<Node> {
	public:
		Node(size_t entID, std::string_view id);
		virtual ~Node();

		void addElement(Node* child);
		void removeElement(std::string_view childId);
		void removeElement(Node* child);

		Node* getParent() const;
		Node* findElement(std::string_view elementId);
		std::vector<Node*> getAllNodes();
		const std::vector<Node*>& getElements();
		Node* getElement(std::string_view elementId);
		std::string_view getId();

	private:
		void getAllNodesHelper(std::vector<Node*>& res);
		void setParent(Node* parentNode);

		Node* parent = nullptr;
		std::vector<Node*> elements;
		std::string id;
	};

}
