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
			data.vboBuf.generate(SFE::GLW::ARRAY_BUFFER);

			data.vao.bind();
			data.vboBuf.bind();
			data.vboBuf.allocateData(mesh->vertices, SFE::GLW::STATIC_DRAW);

			if (!mesh->indices.empty()) {
				data.eboBuf.generate(SFE::GLW::ELEMENT_ARRAY_BUFFER);
				data.eboBuf.bind();
				data.eboBuf.allocateData(mesh->indices, SFE::GLW::STATIC_DRAW);
			}

			data.vao.addAttribute(0, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex3D::position);
			data.vao.addAttribute(1, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex3D::normal);
			data.vao.addAttribute(2, 2, SFE::GLW::AttributeFType::FLOAT, true, &Vertex3D::texCoords);
			data.vao.addAttribute(3, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex3D::tangent);
			data.vao.addAttribute(4, 3, SFE::GLW::AttributeFType::FLOAT, true, &Vertex3D::biTangent);

			data.vao.addAttribute(5, 4, SFE::GLW::AttributeIType::INT, &Vertex3D::boneIDs); //todo only for dynamic mesh
			data.vao.addAttribute(6, 4, SFE::GLW::AttributeFType::FLOAT, false, &Vertex3D::weights);
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