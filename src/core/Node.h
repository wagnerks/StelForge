#pragma once
#include <string_view>
#include <vector>

#include "componentsModule/ComponentHolder.h"

namespace GameEngine::CoreModule {

	class Node : public ComponentsModule::ComponentHolder {
	public:
		Node(std::string_view id);
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
		void setParent(Node* parentNode);

	private:
		void getAllNodesHelper(std::vector<Node*>& res);

		Node* parent = nullptr;
		std::vector<Node*> elements;
		std::string id;
	};

}
