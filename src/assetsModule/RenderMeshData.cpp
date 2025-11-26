#include "RenderMeshData.h"

#include "multithreading/ThreadPool.h"

namespace SFE {
	void RenderMeshData::release() {
		vertexCount = 0;
		indexCount = 0;

		if (Engine::isMainThread()) {
			//Engine::instance()->rhi()->destroyVertexArray(vao);
			Engine::instance()->rhi()->destroyBuffer(vertexBuffer);
			Engine::instance()->rhi()->destroyBuffer(indexBuffer);
		}
		else {
			auto vboBufPtr = vertexBuffer;
			auto eboBufPtr = indexBuffer;
			vertexBuffer = {};
			indexBuffer = {};

			SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([/*vaoPtr,*/ vboBufPtr, eboBufPtr]()mutable {
				//Engine::instance()->rhi()->destroyVertexArray(vaoPtr);
				Engine::instance()->rhi()->destroyBuffer(vboBufPtr);
				Engine::instance()->rhi()->destroyBuffer(eboBufPtr);
			});
		}
	}

	RenderMeshData::~RenderMeshData() {
		//release();
	}
}
