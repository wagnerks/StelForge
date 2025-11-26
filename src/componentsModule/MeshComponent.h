#pragma once
#include "assetsModule/RenderMeshData.h"
#include "assetsModule/modelModule/Model.h"

namespace SFE::ComponentsModule {
	struct MeshComponent {
		Graph<RenderMeshData> meshGraph;

		AssetsModule::Model* meshModel = nullptr;
	};
}

using SFE::ComponentsModule::MeshComponent;