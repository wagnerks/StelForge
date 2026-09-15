#include "MeshLod.h"

#include <meshoptimizer.h>

namespace SFE {
	namespace {
		// Each level aims for this share of the triangles of the level above. Halving is the usual step: the
		// screen size bands below switch at roughly the same rate, so the triangle-to-pixel ratio stays put
		// as a model recedes.
		constexpr float kRatio = 0.5f;
		// Past this the simplifier is collapsing shapes the silhouette is made of, and the model starts to
		// visibly change outline when it switches. It is also where the win stops mattering.
		constexpr size_t kMinIndices = 96;
		constexpr size_t kMaxLevels = 3;
		// In units of the mesh's extent: how far a vertex may move. Generous, because a level only gets used
		// once the model is small on screen, and a strict bound leaves the triangle count where it was.
		constexpr float kErrorPerLevel = 0.05f;
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
			size_t count = meshopt_simplify(simplified.data(), source.data(), source.size(),
				&mesh.vertices[0].position[0], mesh.vertices.size(), sizeof(Vertex3D),
				target, kErrorPerLevel * static_cast<float>(level + 1), 0, &error);
			// A character's index buffer duplicates every vertex on a UV or normal seam, and those duplicates
			// read as the edge of the surface: the simplifier will not collapse across them and stalls after
			// the first level or two. The sloppy pass ignores topology altogether, which is wrong for a model
			// filling the screen and fine for one a few dozen pixels tall, which is all these levels are for.
			if (count == 0 || count > target + target / 4) {
				count = meshopt_simplifySloppy(simplified.data(), source.data(), source.size(),
					&mesh.vertices[0].position[0], mesh.vertices.size(), sizeof(Vertex3D),
					target, kErrorPerLevel * static_cast<float>(level + 2), &error);
			}
			if (count == 0 || count >= source.size()) {
				break;
			}
			simplified.resize(count);
			source = simplified;
			mesh.lodIndices.push_back(std::move(simplified));
		}
	}
}
