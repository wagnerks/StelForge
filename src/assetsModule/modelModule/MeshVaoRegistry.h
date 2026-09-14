#pragma once
#include <mutex>
#include <unordered_map>

#include "Mesh.h"
#include "assetsModule/RenderMeshData.h"
#include "containersModule/Singleton.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE {
	// Every mesh shares one vertex buffer and one index buffer, so a draw differs from the next only in
	// its ranges -- which is what lets one indirect call cover many meshes.
	class MeshVaoRegistry : public Singleton<MeshVaoRegistry> {
	public:
		const RenderMeshData& get(Mesh<Vertex3D>* mesh);
		const RenderMeshData& initMesh(Mesh<Vertex3D>* mesh);

		// Replaced when the buffers grow: fetch per frame, do not cache.
		Render::BufferHandle vertexBuffer() const { return mVertexBuffer; }
		Render::BufferHandle indexBuffer() const { return mIndexBuffer; }

	private:
		void allocate(Mesh<Vertex3D>* mesh, RenderMeshData& data);
		void grow(size_t vertexCapacity, size_t indexCapacity);
		void upload(const Mesh<Vertex3D>* mesh, const RenderMeshData& data) const;

		std::mutex mMutex;
		std::unordered_map<Mesh<Vertex3D>*, RenderMeshData> mMeshVAO;

		Render::BufferHandle mVertexBuffer;
		Render::BufferHandle mIndexBuffer;
		size_t mVertexCapacity = 0;
		size_t mIndexCapacity = 0;
		uint32_t mVertexUsed = 0;
		uint32_t mIndexUsed = 0;
		uint32_t mNextMeshId = 1;
	};
}
