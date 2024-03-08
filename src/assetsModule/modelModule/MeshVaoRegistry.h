#pragma once
#include <unordered_map>

#include "Mesh.h"
#include "assetsModule/RenderMeshData.h"
#include "core/Singleton.h"

namespace SFE {
	class MeshVaoRegistry : public Singleton<MeshVaoRegistry> {
	public:
		const RenderMeshData& get(Mesh<Vertex3D>* mesh);
		const RenderMeshData& initMesh(Mesh<Vertex3D>* mesh);

		void release(Mesh<Vertex3D>* mesh);

	private:
		std::unordered_map<Mesh<Vertex3D>*, RenderMeshData> mMeshVAO;
	};
}
