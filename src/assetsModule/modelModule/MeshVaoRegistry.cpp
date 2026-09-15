#include "MeshVaoRegistry.h"

#include <algorithm>
#include <chrono>

#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "logsModule/logger.h"
#include "multithreading/ThreadPool.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE {
	namespace {
		constexpr size_t kInitialVertices = 1u << 18;
		constexpr size_t kInitialIndices = 1u << 20;
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
		if (data.meshId != 0) {
			return data;
		}

		// The shared buffers are written on the main thread only: the upload queue and a grow both touch
		// state the frame being recorded reads.
		if (SFE::Engine::isMainThread()) {
			allocate(mesh, data);
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

	void MeshVaoRegistry::allocate(Mesh<Vertex3D>* mesh, RenderMeshData& data) {
		const size_t vertexEnd = static_cast<size_t>(mVertexUsed) + mesh->vertices.size();
		const size_t indexEnd = static_cast<size_t>(mIndexUsed) + mesh->indices.size();
		// Offsets are uint32 in RenderMeshData, the mesh table and the draw commands. Past that a mesh is left
		// unuploaded (meshId 0, never drawn) instead of wrapping onto the start of the buffers.
		if (vertexEnd > UINT32_MAX || indexEnd > UINT32_MAX) {
			LogsModule::Logger::LOG_ERROR("MeshVaoRegistry: shared buffers are full (%zu vertices, %zu indices), mesh not uploaded", vertexEnd, indexEnd);
			return;
		}
		if (!mVertexBuffer || vertexEnd > mVertexCapacity || indexEnd > mIndexCapacity) {
			size_t vertexCapacity = mVertexCapacity ? mVertexCapacity : kInitialVertices;
			size_t indexCapacity = mIndexCapacity ? mIndexCapacity : kInitialIndices;
			while (vertexCapacity < vertexEnd) {
				vertexCapacity *= 2;
			}
			while (indexCapacity < indexEnd) {
				indexCapacity *= 2;
			}
			grow(vertexCapacity, indexCapacity);
		}

		data.meshId = mNextMeshId++;
		data.firstVertex = mVertexUsed;
		data.firstIndex = mIndexUsed;
		data.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
		data.indexCount = static_cast<uint32_t>(mesh->indices.size());
		mVertexUsed = static_cast<uint32_t>(vertexEnd);
		mIndexUsed = static_cast<uint32_t>(indexEnd);
		upload(mesh, data);

		GpuMesh entry;
		entry.firstIndex = data.firstIndex;
		entry.indexCount = data.indexCount;
		entry.firstVertex = data.firstVertex;
		entry.vertexCount = data.vertexCount;
		if (!mesh->vertices.empty()) {
			for (int i = 0; i < 3; ++i) {
				entry.boundsMin[i] = entry.boundsMax[i] = mesh->vertices.front().position[i];
			}
			for (const auto& v : mesh->vertices) {
				for (int i = 0; i < 3; ++i) {
					entry.boundsMin[i] = (std::min)(entry.boundsMin[i], v.position[i]);
					entry.boundsMax[i] = (std::max)(entry.boundsMax[i], v.position[i]);
				}
				if (v.boneIDs[0] >= 0) {
					entry.skinned = 1;
				}
			}
		}
		mMeshTable.push_back(entry);
	}

	std::vector<GpuMesh> MeshVaoRegistry::meshTable(const size_t first) {
		std::lock_guard lock(mMutex);
		if (first >= mMeshTable.size()) {
			return {};
		}
		return { mMeshTable.begin() + static_cast<std::ptrdiff_t>(first), mMeshTable.end() };
	}

	size_t MeshVaoRegistry::meshCount() {
		std::lock_guard lock(mMutex);
		return mMeshTable.size();
	}

	void MeshVaoRegistry::grow(const size_t vertexCapacity, const size_t indexCapacity) {
		auto* rhi = Engine::instance()->rhi();
		const auto retire = [rhi](const Render::BufferHandle buffer) {
			if (!buffer) {
				return;
			}
			// Frames already recorded may still draw from it; the RHI keeps it until they have finished.
			rhi->destroyBuffer(buffer);
		};
		retire(mVertexBuffer);
		retire(mIndexBuffer);

		Render::BufferDesc vboDesc;
		vboDesc.usage = Render::BufferUsage::Vertex;
		vboDesc.cpuAccess = Render::CpuAccess::None;
		vboDesc.size = vertexCapacity * sizeof(Vertex3D);
		vboDesc.debugName = "MeshVaoRegistry shared VBO";
		mVertexBuffer = rhi->createBuffer(vboDesc);

		Render::BufferDesc eboDesc;
		eboDesc.usage = Render::BufferUsage::Index;
		eboDesc.cpuAccess = Render::CpuAccess::None;
		eboDesc.size = indexCapacity * sizeof(unsigned int);
		eboDesc.debugName = "MeshVaoRegistry shared EBO";
		mIndexBuffer = rhi->createBuffer(eboDesc);

		mVertexCapacity = vertexCapacity;
		mIndexCapacity = indexCapacity;

		// Offsets stay where they were, so every copy of a RenderMeshData stays valid.
		for (const auto& [mesh, data] : mMeshVAO) {
			if (data.meshId != 0) {
				upload(mesh, data);
			}
		}
	}

	void MeshVaoRegistry::upload(const Mesh<Vertex3D>* mesh, const RenderMeshData& data) const {
		auto* rhi = Engine::instance()->rhi();
		if (!mesh->vertices.empty()) {
			rhi->updateBuffer(mVertexBuffer, static_cast<size_t>(data.firstVertex) * sizeof(Vertex3D),
				mesh->vertices.size() * sizeof(Vertex3D), mesh->vertices.data());
		}
		if (!mesh->indices.empty()) {
			rhi->updateBuffer(mIndexBuffer, static_cast<size_t>(data.firstIndex) * sizeof(unsigned int),
				mesh->indices.size() * sizeof(unsigned int), mesh->indices.data());
		}
	}
}
