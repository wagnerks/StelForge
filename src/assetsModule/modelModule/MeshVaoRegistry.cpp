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

			data.vao.generate();
			data.vboBuf.generate();

			data.vao.bind();
			data.vboBuf.bind();
			data.vboBuf.allocateData(mesh->vertices);

			if (!mesh->indices.empty()) {
				data.eboBuf.generate();
				data.eboBuf.bind();
				data.eboBuf.allocateData(mesh->indices);
			}

			data.vao.addAttribute(0, &Vertex3D::position, true);
			data.vao.addAttribute(1, &Vertex3D::normal, true);
			data.vao.addAttribute(2, &Vertex3D::texCoords, true);
			data.vao.addAttribute(3, &Vertex3D::tangent, true);
			data.vao.addAttribute(4, &Vertex3D::biTangent, true);

			data.vao.addAttribute(5, &Vertex3D::boneIDs); //todo only for dynamic mesh
			data.vao.addAttribute(6, &Vertex3D::weights, false);

			glVertexAttribIPointer(7, 1, GL_UNSIGNED_INT, 0, (void*)0);
			glEnableVertexAttribArray(7);
			glVertexAttribDivisor(7, 1);

			data.vao.bindDefault();

			data.verticesCount = mesh->vertices.size();
			data.indicesCount = mesh->indices.size();
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