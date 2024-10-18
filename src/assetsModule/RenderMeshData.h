#pragma once

#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"
#include "modelModule/Vertex.h"

namespace SFE {
	struct RenderMeshData {
		GLW::VertexArray vao;
		GLW::Buffer<GLW::ARRAY_BUFFER, Vertex3D> vboBuf;
		GLW::Buffer<GLW::ELEMENT_ARRAY_BUFFER, unsigned int> eboBuf;

		size_t verticesCount = 0;
		size_t indicesCount = 0;
		void release();
		~RenderMeshData();
	};
}