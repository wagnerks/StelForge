#pragma once

namespace AssetsModule {
	class Mesh;
}

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
		static AssetsModule::Mesh* createPrimitiveMesh(eDrawObjectType type);
	};


}
