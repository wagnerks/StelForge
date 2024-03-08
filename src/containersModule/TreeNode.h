#pragma once
#include <functional>
#include <string_view>
#include <vector>

namespace SFE::NodeModule {
	template<class T>
	class TreeNode {
	public:
		TreeNode(const TreeNode& other) = delete;
		TreeNode& operator=(const TreeNode& other) = delete;

		TreeNode(TreeNode&& other) noexcept
			: mOnElementRemove(std::move(other.mOnElementRemove)),
			mOnElementAdd(std::move(other.mOnElementAdd)),
			mParent(other.mParent),
			mElements(std::move(other.mElements)),
			mNodeId(std::move(other.mNodeId)) {

			for (auto element : mElements) {
				other.removeElement(element);
				element->setParent(static_cast<T*>(this));

				if (mOnElementAdd) {
					mOnElementAdd(element);
				}
			}
		}

		TreeNode& operator=(TreeNode&& other) noexcept {
			if (this == &other)
				return *this;

			mOnElementRemove = std::move(other.mOnElementRemove);
			mOnElementAdd = std::move(other.mOnElementAdd);
			mParent = other.mParent;
			mElements = std::move(other.mElements);
			mNodeId = std::move(other.mNodeId);

			for (auto element : mElements) {
				other.removeElement(element);
				element->setParent(static_cast<T*>(this));

				if (mOnElementAdd) {
					mOnElementAdd(element);
				}
			}
			return *this;
		}

		TreeNode() = default;
		virtual ~TreeNode();

		void addElement(T* child);
		void removeElement(std::string_view childId);
		void removeElement(T* child);


		T* getParent() const;
		T* findElement(std::string_view elementId);
		std::vector<T*> getAllNodes();
		const std::vector<T*>& getElements();
		T* getElement(std::string_view elementId);

		bool empty() {
			return mElements.empty();
		}
		size_t size() {
			return mElements.size();
		}
		std::string_view getNodeId();
		void setNodeId(std::string_view id);

	protected:
		std::function<void(T*)> mOnElementRemove = nullptr;
		std::function<void(T*)> mOnElementAdd = nullptr;

	private:
		void setParent(T* parentNode);
		void getAllNodesHelper(std::vector<T*>& res);


		T* mParent = nullptr;
		std::vector<T*> mElements;

		std::string mNodeId;

	};

	template <class T>
	TreeNode<T>::~TreeNode() {
		if (auto parent = getParent()) {
			parent->removeElement(static_cast<T*>(this));
		}

		auto elements = getElements();
		for (auto element : elements) {
			delete element;
		}
	}

	template <class T>
	void TreeNode<T>::addElement(T* child) {
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
	void TreeNode<T>::removeElement(std::string_view childId) {
		if (const auto child = getElement(childId)) {
			removeElement(child);
		}
	}

	template <class T>
	void TreeNode<T>::removeElement(T* child) {
		std::erase(mElements, child);
		child->setParent(nullptr);
		if (mOnElementRemove) {
			mOnElementRemove(child);
		}
	}

	template <class T>
	T* TreeNode<T>::getParent() const {
		return mParent;
	}

	template <class T>
	T* TreeNode<T>::findElement(std::string_view elementId) {
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
	std::vector<T*> TreeNode<T>::getAllNodes() {
		std::vector<T*> res;
		getAllNodesHelper(res);
		return res;
	}

	template <class T>
	const std::vector<T*>& TreeNode<T>::getElements() {
		return mElements;
	}

	template <class T>
	T* TreeNode<T>::getElement(std::string_view elementId) {
		auto it = std::ranges::find_if(mElements, [elementId](T* child) {
			return elementId == child->getNodeId();
		});
		if (it != mElements.end()) {
			return *it;
		}
		return nullptr;
	}

	template <class T>
	std::string_view TreeNode<T>::getNodeId() {
		return mNodeId;
	}

	template <class T>
	void TreeNode<T>::setNodeId(std::string_view id) {
		mNodeId = id;
	}

	template <class T>
	void TreeNode<T>::getAllNodesHelper(std::vector<T*>& res) {
		res.insert(res.end(), mElements.begin(), mElements.end());
		for (auto element : mElements) {
			element->getAllNodesHelper(res);
		}
	}

	template <class T>
	void TreeNode<T>::setParent(T* parentNode) {
		mParent = parentNode;
	}
}
