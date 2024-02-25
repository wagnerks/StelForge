#pragma once
#include <forward_list>
#include <functional>
#include <string_view>
#include <vector>

namespace SFE {
	template<class Data>
	struct Tree {
		Tree(const Tree& other)
			: value(other.value),
			  children(other.children),
			  parent(nullptr) {
			for (auto& child : children) {
				child.parent = this;
			}
		}

		Tree(Tree&& other) noexcept
			: value(std::move(other.value)),
			  children(std::move(other.children)),
			  parent(nullptr) {
			for (auto& child : children) {
				child.parent = this;
			}
		}

		Tree& operator=(const Tree& other) {
			if (this == &other)
				return *this;
			value = other.value;
			children = other.children;
			for (auto& child : children) {
				child.parent = this;
			}
			return *this;
		}

		Tree& operator=(Tree&& other) noexcept {
			if (this == &other)
				return *this;

			value = std::move(other.value);
			children = std::move(other.children);
			for (auto& child : children) {
				child.parent = this;
			}
			return *this;
		}

	public:
		class Iterator {
		public:
			Iterator(Tree* tree) {
				mCurrentNode = tree;
			}

			Tree& operator*() {
				return *mCurrentNode;
			}

			Iterator& operator++() {
				if (mPosition.empty()) {
					if (mCurrentNode->children.empty()) {
						mCurrentNode = nullptr;
						return *this;
					}

					mPosition.push_back(mCurrentNode->children.begin());
					mCurrentNode = &*mPosition.back();

					return *this;
				}

				if (!mCurrentNode->children.empty() && !mVisited.contains(mCurrentNode)) {
					mPosition.push_back(mCurrentNode->children.begin());
					mCurrentNode = &*mPosition.back();

					return *this;
				}

				while (++mPosition.back() == mCurrentNode->parent->children.end()) {
					mPosition.pop_back();
					if (mPosition.empty()) {
						mCurrentNode = nullptr;
						return *this;
					}

					mVisited.insert(mCurrentNode->parent);

					mCurrentNode = &*(mPosition.back());
					auto next = mPosition.back();
					++next;
					if (next == mCurrentNode->parent->children.end()) {
						continue;
					}

					mCurrentNode = &*(next);
					return *this;
				}

				mCurrentNode = &*mPosition.back();
				return *this;
			}

			bool operator!=(const Iterator& other) const { return other.mCurrentNode != mCurrentNode; }

		private:
			std::vector<typename std::forward_list<Tree>::iterator> mPosition;
			std::set<Tree*> mVisited;
			Tree* mCurrentNode = nullptr;
		};

		Iterator begin() { return Iterator(this); }
		Iterator end() { return Iterator(nullptr); }

	public:
		Tree() = default;
		Tree(Data&& data, Tree* parent) : value(std::forward<Data>(data)), parent(parent) {}

		Tree& addChild(Data&& data) {
			return children.emplace_front(std::forward<Data>(data), this);
		}

		std::forward_list<Tree>& getChildren() {
			return children;
		}

	public:
		Data value;

		std::forward_list<Tree> children;
		Tree* parent = nullptr;
	};
}