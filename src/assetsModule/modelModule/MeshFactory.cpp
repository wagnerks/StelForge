#include "MeshFactory.h"

#include <cassert>

#include "Mesh.h"
#include "Model.h"

using namespace SFE::ModelModule;

AssetsModule::Mesh* MeshFactory::createPrimitiveMesh(eDrawObjectType type) {
	AssetsModule::Mesh* res = nullptr;
	std::vector<AssetsModule::Vertex> vertices;
	std::vector<unsigned> indices;

	switch (type) {
	case eDrawObjectType::CUBE:
		vertices = {
			// back face
			{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // bottom-left
			{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}, // top-right
			{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}}, // bottom-right         
			{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}, // top-right
			{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // bottom-left
			{{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}}, // top-left
			// front face
			{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // bottom-left
			{{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // bottom-right
			{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // top-right
			{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // top-right
			{{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // top-left
			{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // bottom-left
			// left face
			{{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // top-right
			{{-1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // top-left
			{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // bottom-left
			{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // bottom-left
			{{-1.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // bottom-right
			{{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // top-right
			// right face
			{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // top-left
			{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // bottom-right
			{{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // top-right         
			{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // bottom-right
			{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // top-left
			{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // bottom-left     
			// bottom face
			{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, // top-right
			{{1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}, // top-left
			{{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // bottom-left
			{{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // bottom-left
			{{-1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // bottom-right
			{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, // top-right
			// top face
			{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // top-left
			{{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // bottom-right
			{{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // top-right     
			{{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // bottom-right
			{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // top-left
			{{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}} // bottom-left
		};
		break;
	case eDrawObjectType::TRIANGLE:
		vertices = {
			{{-0.5f, -0.5f, 0.f}, {}, {0.0f, 0.0f}},
			{{0.5f, -0.5f, 0.f}, {}, {1.0f, 0.0f}},
			{{0.5f, 0.5f, 0.f}, {}, {1.0f, 1.0f}}
		};

		indices = {
			0, 1, 2
		};
		break;
	case eDrawObjectType::SQUARE:
		vertices = {
			{{ 0.5f,  0.5f, 0.0f},{0.f,0.f,1.f}, {1.0f, 1.0f}}, // top right
			{{ 0.5f, -0.5f, 0.0f},{0.f,0.f,1.f}, {1.0f, 0.0f}},  // bottom right
			{{-0.5f, -0.5f, 0.0f},{0.f,0.f,1.f}, {0.0f, 0.0f}}, // bottom left
			{{-0.5f,  0.5f, 0.0f},{0.f,0.f,1.f}, {0.0f, 1.0f}}  // top left
		};

		indices = {
			0,1,2,
			0,2,3
		};

		break;
	case eDrawObjectType::PRIMITIVE_CUBE:
		vertices = {
			{{0.5f,  0.5f, 0.5f},{}, {1.0f, 1.0f}}, // top right
			{{0.5f, -0.5f, 0.5f},{}, {1.0f, 0.0f}},  // bottom right
			{{-0.5f, -0.5f, 0.5f},{}, {0.0f, 0.0f}}, // bottom left
			{{-0.5f,  0.5f, 0.5f},{}, {0.0f, 1.0f}},  // top left

			{{-0.5f,  0.5f, -0.5f},{}, {0.0f, 1.0f}},  //4 left top back
			{{-0.5f, -0.5f, -0.5f},{}, {0.0f, 1.0f}},  //5 left btm back

			{{0.5f,  0.5f, -0.5f},{}, {0.0f, 1.0f}}, //6 right top back
			{{0.5f, -0.5f, -0.5f},{}, {0.0f, 1.0f}}  //7 right btm back
		};

		indices = {
		   0,1,3,
		   1,2,3,

		   3,2,4,
		   4,2,5,

		   4,7,6,
		   4,5,7,

		   0,1,6,
		   6,1,7,

		   0,3,4,
		   0,4,6,

		   1,2,5,
		   1,5,7,
		};

		break;
	case eDrawObjectType::PRIMITIVE_CUBE_N:
		vertices = {
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f},{}},
			{{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f},{}},
			{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f},{}},
			{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f},{}},
			{{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f},{}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f},{}},

			{{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},{}},
			{{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},{}},
			{{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},{}},
			{{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},{}},
			{{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},{}},
			{{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f},{}},

			{{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f},{}},
			{{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f},{}},
			{{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f},{}},
			{{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f},{}},
			{{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f},{}},
			{{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f},{}},

			{{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f},{}},
			{{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f},{}},
			{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f},{}},
			{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f},{}},
			{{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f},{}},
			{{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f},{}},

			{{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f},{}},
			{{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f},{}},
			{{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f},{}},
			{{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f},{}},
			{{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f},{}},
			{{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f},{}},

			{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{}},
			{{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f},{}},
			{{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f},{}},
			{{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f},{}},
			{{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f},{}},
			{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {}}
		};
		break;
	default:
		assert(false);
	}

	res = new AssetsModule::Mesh(vertices, indices);
	return res;
}