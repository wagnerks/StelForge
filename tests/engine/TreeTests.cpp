#include <string>

#include <gtest/gtest.h>

#include "containersModule/Tree.h"

// The walk over a Tree, as models walk their meshes: every node once, parents before their children. A node after a
// sibling with children of its own was once come to two or three times, and a model's meshes handed out twice were
// then worked on by two threads at once (ModelLoader::finishMeshes).
namespace SFE::TreeTests {
	// R -> { A -> { A1 }, B -> { B1 }, C }; addChild puts a child in front, so they are walked C, B, A.
	Tree<std::string> sample() {
		Tree<std::string> root;
		root.value = "R";
		root.addChild("A").addChild("A1");
		root.addChild("B").addChild("B1");
		root.addChild("C");
		return root;
	}

	TEST(Tree, WalksEveryNodeOnce) {
		auto root = sample();
		std::string order;
		for (auto& node : root) {
			order += node.value + " ";
		}
		EXPECT_EQ(order, "R C B B1 A A1 ");
	}

	TEST(Tree, WalksEveryNodeOnceWhenConst) {
		const auto root = sample();
		std::string order;
		for (const auto& node : root) {
			order += node.value + " ";
		}
		EXPECT_EQ(order, "R C B B1 A A1 ");
	}
}
