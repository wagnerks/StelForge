#include "MeshVaoRegistry.h"

#include "core/Engine.h"
#include "assetsModule/RenderMeshData.h"
#include "multithreading/ThreadPool.h"

namespace SFE {
	const RenderMeshData& MeshVaoRegistry::get(Mesh<Vertex3D>* mesh) {
		assert(mesh);
		auto res = mMeshVAO.find(mesh);
		if (res != mMeshVAO.end()) {
			return res->second;
		}

		return initMesh(mesh);
	}

	const RenderMeshData& MeshVaoRegistry::initMesh(Mesh<Vertex3D>* mesh) {
		assert(mesh);

		auto& data = mMeshVAO[mesh];

		if (SFE::Engine::isMainThread()) {
			data.release();

			Render::BufferDesc vboBuf;
			vboBuf.usage = Render::BufferUsage::Vertex;
			vboBuf.cpuAccess = Render::CpuAccess::None;
			vboBuf.size = mesh->vertices.size() * sizeof(Vertex3D);
			vboBuf.debugName = "VBO MeshVaoRegistry";

			data.vertexBuffer = Engine::instance()->rhi()->createBuffer(vboBuf, mesh->vertices.data());
			if (!mesh->indices.empty()) {
				Render::BufferDesc eboBuf;
				eboBuf.usage = Render::BufferUsage::Index;
				eboBuf.cpuAccess = Render::CpuAccess::None;
				eboBuf.size = mesh->indices.size() * sizeof(unsigned int);
				eboBuf.debugName = "EBO MeshVaoRegistry";
				data.indexBuffer = Engine::instance()->rhi()->createBuffer(eboBuf, mesh->indices.data());
			}

			data.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
			data.indexCount = static_cast<uint32_t>(mesh->indices.size());
		}
		else {
			auto futur = ThreadPool::instance()->addTask<WorkerType::SYNC>([mesh]()mutable { //easy crash 
				MeshVaoRegistry::instance()->initMesh(mesh);
			});
			futur.get();
		}

		return data;
	}

	void MeshVaoRegistry::release(Mesh<Vertex3D>* mesh) {
		if (!mesh) {
			return;
		}

		mMeshVAO.erase(mesh);
	}
}