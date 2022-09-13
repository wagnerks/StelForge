#pragma once
#include <functional>
#include <string_view>
#include <vector>

namespace GameEngine::NodeModule {
	template<class T>
	class Node {
	public:
		virtual ~Node();;

		void addElement(T* child);
		void removeElement(std::string_view childId);
		void removeElement(T* child);
		

		T* getParent() const;
		T* findElement(std::string_view elementId);
		std::vector<T*> getAllNodes();
		const std::vector<T*>& getElements();
		T* getElement(std::string_view elementId);
		std::string_view getNodeId();
		void setNodeId(std::string_view id);
		void setParent(T* parentNode);
	protected:
		std::function<void(T*)> mOnElementRemove = nullptr;
		std::function<void(T*)> mOnElementAdd = nullptr;

	private:
		void getAllNodesHelper(std::vector<T*>& res);
		

		T* mParent = nullptr;
		std::vector<T*> mElements;

		std::string mNodeId;
		
	};

	template <class T>
	Node<T>::~Node() {
		for (auto& element : getElements()) {
			delete element;
		}
	}

	template <class T>
	void Node<T>::addElement(T* child) {
		if (auto childParent = child->getParent()) {
			childParent->removeElement(child);
		}

		child->setParent(static_cast<T*>(this));
		mElements.emplace_back(child);
		if (mOnElementAdd) {
			mOnElementAdd(child);
		}
		
	}

	template <class T>
	void Node<T>::removeElement(std::string_view childId) {
		if (const auto child = getElement(childId)) {
			removeElement(child);
		}
	}

	template <class T>
	void Node<T>::removeElement(T* child) {
		std::erase(mElements, child);
		child->setParent(nullptr);
		if (mOnElementRemove) {
			mOnElementRemove(child);
		}
		
	}

	template <class T>
	T* Node<T>::getParent() const {
		return mParent;
	}

	template <class T>
	T* Node<T>::findElement(std::string_view elementId) {
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

	template <class T>
	std::vector<T*> Node<T>::getAllNodes() {
		std::vector<T*> res;
		getAllNodesHelper(res);
		return res;
	}

	template <class T>
	const std::vector<T*>& Node<T>::getElements() {
		return mElements;
	}

	template <class T>
	T* Node<T>::getElement(std::string_view elementId) {
		auto it = std::ranges::find_if(mElements, [elementId](T* child) {
			return elementId == child->getStringId();
		});
		if (it != mElements.end()) {
			return *it;
		}
		return nullptr;
	}

	template <class T>
	std::string_view Node<T>::getNodeId() {
		return mNodeId;
	}

	template <class T>
	void Node<T>::setNodeId(std::string_view id) {
		mNodeId = id;
	}

	template <class T>
	void Node<T>::getAllNodesHelper(std::vector<T*>& res) {
		res.insert(res.end(), mElements.begin(), mElements.end());
		for (auto element : mElements) {
			element->getAllNodesHelper(res);
		}
	}

	template <class T>
	void Node<T>::setParent(T* parentNode) {
		mParent = parentNode;
	}
}
