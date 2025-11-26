#pragma once


#include "renderModule/RenderHardwareInterface.h"

namespace SFE {
	struct RenderMeshData {
		friend bool operator<(const RenderMeshData& lhs, const RenderMeshData& rhs)
		{
			return lhs.vertexBuffer < rhs.vertexBuffer;
		}

		friend bool operator<=(const RenderMeshData& lhs, const RenderMeshData& rhs)
		{
			return !(rhs < lhs);
		}

		friend bool operator>(const RenderMeshData& lhs, const RenderMeshData& rhs)
		{
			return rhs < lhs;
		}

		friend bool operator>=(const RenderMeshData& lhs, const RenderMeshData& rhs)
		{
			return !(lhs < rhs);
		}

		friend bool operator==(const RenderMeshData& lhs, const RenderMeshData& rhs)
		{
			return lhs.vertexBuffer == rhs.vertexBuffer;
		}

		friend bool operator!=(const RenderMeshData& lhs, const RenderMeshData& rhs)
		{
			return !(lhs == rhs);
		}

		//todo  guarantee lifetime somehow
		Render::BufferHandle    vertexBuffer;
		Render::BufferHandle    indexBuffer;

		Render::IndexType   indexType = Render::IndexType::Uint32;

		uint32_t            vertexOffsetBytes = 0; // смещение в VBO
		uint32_t            indexOffsetBytes = 0; // смещение в IBO

		uint32_t            vertexCount = 0;
		uint32_t 			indexCount = 0;

		void release();
		~RenderMeshData();
	};
}
