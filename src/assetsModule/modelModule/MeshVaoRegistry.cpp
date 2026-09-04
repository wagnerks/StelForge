#include "MeshVaoRegistry.h"

#include <chrono>

#include "core/Engine.h"
#include "assetsModule/RenderMeshData.h"
#include "multithreading/ThreadPool.h"
#include "renderModule/GraphicsBackend.h"

namespace SFE {
	namespace {
		void uploadMeshBuffers(Mesh<Vertex3D>* mesh, RenderMeshData& data) {
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
	}
	const RenderMeshData& MeshVaoRegistry::get(Mesh<Vertex3D>* mesh) {
		assert(mesh);
		return initMesh(mesh);
	}

	const RenderMeshData& MeshVaoRegistry::initMesh(Mesh<Vertex3D>* mesh) {
		assert(mesh);

		std::unique_lock lock(mMutex);
		auto& data = mMeshVAO[mesh];

		if (mesh->vertices.empty() && mesh->indices.empty()) {
			return data;
		}
		if (data.vertexBuffer) {
			return data;
		}

		const bool vulkanBackend = Render::activeGraphicsBackend() == Render::GraphicsBackend::Vulkan;
		if (SFE::Engine::isMainThread() || vulkanBackend) {
			uploadMeshBuffers(mesh, data);
			return data;
		}

		lock.unlock();
		auto futur = ThreadPool::instance()->addTask<WorkerType::SYNC>([mesh]() {
			MeshVaoRegistry::instance()->initMesh(mesh);
		});
		if (futur.valid()) {
			while (futur.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
				if (!ThreadPool::isAlive() || ThreadPool::instance()->isShuttingDown()) {
					break;
				}
			}
		}
		lock.lock();
		return mMeshVAO[mesh];
	}

	void MeshVaoRegistry::release(Mesh<Vertex3D>* mesh) {
		if (!mesh) {
			return;
		}
		std::lock_guard lock(mMutex);
		mMeshVAO.erase(mesh);
	}
}