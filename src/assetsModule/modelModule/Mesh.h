#pragma once

#include <vector>

#include "Material.h"
#include "Vertex.h"

namespace SFE {
	template<class VertexType>
	struct Mesh {
		Mesh() = default;
		Mesh(std::vector<VertexType>&& vertices, std::vector<unsigned int>&& indices = {}) : indices(std::move(indices)), vertices(std::move(vertices)){}

		std::vector<unsigned int> indices;
		std::vector<VertexType> vertices;
	};

	using Mesh3D = Mesh<Vertex3D>;
	using Mesh2D = Mesh<Vertex2D>;

	template<class VertexType>
	struct MeshObject {
		Mesh<VertexType> mesh;
		Material material;
		Math::Mat4 transform;
	};

	using MeshObject3D = MeshObject<Vertex3D>;
	using MeshObject2D = MeshObject<Vertex2D>;
}
