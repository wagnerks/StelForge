#pragma once
#include "Mesh.h"

namespace SFE::ModelModule {
	enum class eDrawObjectType {
		CUBE = 0,
		TRIANGLE,
		SQUARE,
		PRIMITIVE_CUBE,
		PRIMITIVE_CUBE_N
	};

	class MeshFactory {
	public:
		static Mesh3D createPrimitiveMesh(eDrawObjectType type);
	};
}
