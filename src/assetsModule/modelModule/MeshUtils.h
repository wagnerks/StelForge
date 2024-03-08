#pragma once
#include <map>

#include "Mesh.h"

namespace SFE {
	struct MeshUtils {
		static void recalculateNormals(Mesh<Vertex3D>* mesh, bool smooth);

		static void recalculateFaceNormal(Vertex3D* a, Vertex3D* b, Vertex3D* c);
		static void recalculateFaceNormals(Mesh<Vertex3D>* mesh);
		static void recalculateVerticesNormals(Mesh<Vertex3D>* mesh);
		static void recalculateVerticesNormals(std::map<SFE::Math::Vec3, std::vector<Vertex3D*>>& vertices);
	};
}
