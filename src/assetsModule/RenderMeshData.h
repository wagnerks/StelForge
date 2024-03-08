#pragma once

#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"

namespace SFE {
	struct RenderMeshData {
		GLW::VertexArray vao;
		GLW::Buffer vboBuf;
		GLW::Buffer eboBuf;

		size_t verticesCount = 0;
		size_t indicesCount = 0;
		void release();
		~RenderMeshData();
	};
}