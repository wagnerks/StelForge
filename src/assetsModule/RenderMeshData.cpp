#include "RenderMeshData.h"

#include "multithreading/ThreadPool.h"

namespace SFE {
	void RenderMeshData::release() {
		verticesCount = 0;
		indicesCount = 0;

		if (Engine::isMainThread()) {
			vao.release();
			vboBuf.release();
			eboBuf.release();
		}
		else {
			auto vaoPtr = new GLW::VertexArray(std::move(vao));
			auto vboBufPtr = new GLW::Buffer(std::move(vboBuf));
			auto eboBufPtr = new GLW::Buffer(std::move(eboBuf));
			SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([vaoPtr, vboBufPtr, eboBufPtr]()mutable {
				delete vaoPtr;
				delete vboBufPtr;
				delete eboBufPtr;
			});
		}
	}

	RenderMeshData::~RenderMeshData() {
		release();
	}
}
