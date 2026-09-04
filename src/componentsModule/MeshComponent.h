#pragma once
#include <vector>

#include "assetsModule/RenderMeshData.h"
#include "assetsModule/modelModule/Model.h"
#include "componentsModule/MaterialComponent.h"
#include "ecss/Types.h"

namespace SFE::ComponentsModule {
	struct MeshComponent {
		Graph<RenderMeshData> meshGraph;
		std::vector<MaterialComponent> meshMaterials;
		AssetsModule::Model* meshModel = nullptr;

		template<typename Fn>
		void forEachDrawMesh(const MaterialComponent* entityMat, Fn&& fn) const {
			size_t i = 0;
			for (const auto& node : meshGraph) {
				const MaterialComponent* mat = entityMat;
				if (i < meshMaterials.size()) {
					mat = &meshMaterials[i];
				}
				++i;
				if (!node.value.vertexBuffer || (node.value.vertexCount == 0 && node.value.indexCount == 0)) {
					continue;
				}
				fn(node.value, mat);
			}
		}
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::MeshComponent> : std::true_type {};

using SFE::ComponentsModule::MeshComponent;