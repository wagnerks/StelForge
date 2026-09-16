#include "MeshLod.h"

#include <meshoptimizer.h>

namespace SFE {
	namespace {
		// Each level aims for this share of the triangles of the level above.
		constexpr float kRatio = 0.5f;
		// Past this the simplifier is collapsing the shapes the silhouette is made of, and the win no longer
		// matters anyway.
		constexpr size_t kMinIndices = 96;
		constexpr size_t kMaxLevels = 3;
		// A level has to be meaningfully cheaper than the one above to be worth an id and an instance slot.
		constexpr float kKeepBelow = 0.9f;
		// Fraction of the mesh's size a vertex may move, per level. Generous: a level is only used once the
		// model is small on screen.
		constexpr float kErrorPerLevel = 0.03f;
		// texCoords and normal sit next to each other in Vertex3D, so one pointer covers both. Collapsing on
		// position alone folds a shoulder into a sleeve and drags the texture across the seam with it, which
		// reads as a grey smear on a model at middle distance.
		constexpr size_t kAttributeCount = 5;
		constexpr float kAttributeWeights[kAttributeCount] = { 0.5f, 0.5f, 0.25f, 0.25f, 0.25f };
	}

	void weldMesh(Mesh<Vertex3D>& mesh) {
		if (mesh.indices.empty() || mesh.vertices.empty()) {
			return;
		}
		std::vector<unsigned int> remap(mesh.vertices.size());
		const size_t unique = meshopt_generateVertexRemap(remap.data(), mesh.indices.data(), mesh.indices.size(),
			mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex3D));
		if (unique >= mesh.vertices.size()) {
			return;
		}
		std::vector<unsigned int> indices(mesh.indices.size());
		meshopt_remapIndexBuffer(indices.data(), mesh.indices.data(), mesh.indices.size(), remap.data());
		std::vector<Vertex3D> vertices(unique);
		meshopt_remapVertexBuffer(vertices.data(), mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex3D), remap.data());
		mesh.indices = std::move(indices);
		mesh.vertices = std::move(vertices);
	}

	void buildMeshLods(Mesh<Vertex3D>& mesh) {
		mesh.lodIndices.clear();
		if (mesh.indices.size() < kMinIndices * 2 || mesh.vertices.empty()) {
			return;
		}

		std::vector<unsigned int> source = mesh.indices;
		for (size_t level = 0; level < kMaxLevels; ++level) {
			const size_t target = static_cast<size_t>(static_cast<float>(source.size()) * kRatio) / 3u * 3u;
			if (target < kMinIndices) {
				break;
			}
			std::vector<unsigned int> simplified(source.size());
			float error = 0.f;
			const size_t count = meshopt_simplifyWithAttributes(simplified.data(), source.data(), source.size(),
				&mesh.vertices[0].position[0], mesh.vertices.size(), sizeof(Vertex3D),
				&mesh.vertices[0].texCoords[0], sizeof(Vertex3D), kAttributeWeights, kAttributeCount, nullptr,
				target, kErrorPerLevel * static_cast<float>(level + 1), 0, &error);
			// It stops early on a mesh it cannot collapse further without breaking the surface. Whatever it
			// did reach is the last level: another one would be the same triangles under a different id.
			if (count == 0 || static_cast<float>(count) > static_cast<float>(source.size()) * kKeepBelow) {
				break;
			}
			simplified.resize(count);
			source = simplified;
			mesh.lodIndices.push_back(std::move(simplified));
		}
	}
}
