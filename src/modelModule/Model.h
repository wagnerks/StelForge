#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"


namespace GameEngine::ModelModule {
	struct MeshNode : NodeModule::Node<MeshNode> {
		MeshNode() = default;
		MeshNode(MeshNode&& other) noexcept
			: NodeModule::Node<MeshNode>(std::move(other)),
			  mMeshes(std::move(other.mMeshes)) {
			auto elements = other.getElements();
			for (auto element : elements) {
				other.removeElement(element);
			}
		}

		MeshNode& operator=(MeshNode&& other) noexcept {
			if (this == &other)
				return *this;
			NodeModule::Node<MeshNode>::operator =(std::move(other));
			mMeshes = std::move(other.mMeshes);
			auto elements  = other.getElements();
			for (auto element : elements) {
				other.removeElement(element);
			}
			return *this;
		}

		std::unordered_map<size_t, std::vector<Mesh>> mMeshes;
	};

	class Model : public ecsModule::Entity<Model> {
	public:
		Model(size_t entID, MeshNode& model) : Entity<Model>(entID) {
			mMeshTree = std::move(model);
		}

		MeshNode mMeshTree;
	private:
		
	};
}
