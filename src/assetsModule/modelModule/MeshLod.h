#pragma once

#include "Mesh.h"
#include "Vertex.h"

namespace SFE {
	// Fills mesh.lodIndices with coarser index sets over the mesh's own vertices. Index-only: a vertex is
	// either kept with everything it carries -- bone weights included -- or no longer referenced, so a
	// skinned mesh simplifies without touching the skinning.
	void buildMeshLods(Mesh<Vertex3D>& mesh);
}
