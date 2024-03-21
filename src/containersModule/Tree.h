#pragma once
#include <forward_list>
#include <functional>

namespace SFE {
	template<class Data>
	struct Tree { //todo custom allocator and fast iteration, store data sequentially
		//the problem is that reference on parent should be updated every add child if data was reallocated
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

		class ConstIterator {
		public:
			ConstIterator(const Tree* tree) {
				mCurrentNode = tree;
			}

			const Tree& operator*() const {
				return *mCurrentNode;
			}

			ConstIterator& operator++() {
				if (mPosition.empty()) {
					if (mCurrentNode->children.empty()) {
						mCurrentNode = nullptr;
						return *this;
					}

					mPosition.push_back(mCurrentNode->children.cbegin());
					mCurrentNode = &*mPosition.back();

					return *this;
				}

				if (!mCurrentNode->children.empty() && !mVisited.contains(mCurrentNode)) {
					mPosition.push_back(mCurrentNode->children.cbegin());
					mCurrentNode = &*mPosition.back();

					return *this;
				}

				while (++mPosition.back() == mCurrentNode->parent->children.cend()) {
					mPosition.pop_back();
					if (mPosition.empty()) {
						mCurrentNode = nullptr;
						return *this;
					}

					mVisited.insert(mCurrentNode->parent);

					mCurrentNode = &*(mPosition.back());
					auto next = mPosition.back();
					++next;
					if (next == mCurrentNode->parent->children.cend()) {
						continue;
					}

					mCurrentNode = &*(next);
					return *this;
				}

				mCurrentNode = &*mPosition.back();
				return *this;
			}

			bool operator!=(const ConstIterator& other) const { return other.mCurrentNode != mCurrentNode; }

		private:
			std::vector<typename std::forward_list<Tree>::const_iterator> mPosition;
			std::set<const Tree*> mVisited;
			const Tree* mCurrentNode = nullptr;
		};

		Iterator begin() { return Iterator(this); }
		Iterator end() { return Iterator(nullptr); }

		ConstIterator begin() const { return ConstIterator(this); }
		ConstIterator end() const { return ConstIterator(nullptr); }

	public:
		Tree() = default;
		Tree(Data&& data, Tree* parent) : value(std::forward<Data>(data)), parent(parent) {}

		Tree& addChild(Data&& data) {
			return children.emplace_front(std::forward<Data>(data), this);
		}

		std::forward_list<Tree>& getChildren() {
			return children;
		}

		template<typename OtherT>
		void fillTree(const SFE::Tree<OtherT>& sourceNode, std::function<Data(const OtherT&)> extractData) {
			value = extractData(sourceNode.value);
			fillTreeImpl<OtherT>(sourceNode, extractData);
		}

		Data value;

	public:
		
		std::forward_list<Tree> children;
		Tree* parent = nullptr;

	private:
		template<typename OtherT>
		void fillTreeImpl(const SFE::Tree<OtherT>& sourceNode, std::function<Data(const OtherT&)> extractData) {
			for (auto& child : sourceNode.children) {
				addChild(std::move(extractData(child.value)));
				children.front().template fillTreeImpl<OtherT>(child, extractData);
			}
		}
	};

	template<class Data>
	struct Graph {
		Graph() = default;
		Graph(const Tree<Data>& treeRef) {
			std::function<void(const Tree<Data>&, int)> helper;
			helper = [&helper, this](const Tree<Data>& node, int parent) {
				for (auto& child : node.children) {
					addChild(child.value, parent);
					helper(child, tree[parent].children.back());
				}
			};

			tree[0].value = treeRef.value;
			helper(treeRef, 0);
		}

		template<typename OtherT>
		void fill(const Tree<OtherT>& treeRef, std::function<Data(const OtherT&)> extractData) {
			std::function<void(const Tree<OtherT>&, int)> helper;
			helper = [&helper, this, &extractData](const Tree<OtherT>& node, int parent) {
				for (auto& child : node.children) {
					addChild(extractData(child.value), parent);
					helper(child, tree[parent].children.back());
				}
			};

			tree[0].value = extractData(treeRef.value);
			helper(treeRef, 0);
		}

		struct Node {
			Data value = {};
			int parent = 0;
			std::vector<size_t> children;
		};

		void addChild(Data&& data, int parent = 0) {
			tree.emplace_back(std::forward<Data>(data), parent);
			tree[parent].children.push_back(tree.size() - 1);
		}

		typename std::vector<Node>::iterator begin() { return tree.begin(); }
		typename std::vector<Node>::iterator end() { return tree.end(); }

		typename std::vector<Node>::const_iterator begin() const { return tree.cbegin(); }
		typename std::vector<Node>::const_iterator end() const { return tree.cend(); }

		Node& root() { return tree[0]; }
	private:

		std::vector<Node> tree = { {} };
	};
}
