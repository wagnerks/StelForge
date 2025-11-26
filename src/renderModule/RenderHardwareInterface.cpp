#include <renderModule/RenderHardwareInterface.h>

#include "core/Engine.h"

namespace SFE::Render
{

	void BufferHandle::bindVertex(uint32_t slot, uint32_t offset) const
	{
		Engine::instance()->rhi()->bindVertexBuffer(slot, *this, offset);
	}

	void BufferHandle::bindIndex(IndexType type, uint32_t offset) const
	{
		Engine::instance()->rhi()->bindIndexBuffer(*this, type, offset);
	}

	void BufferHandle::bindUniform(uint32_t binding) const
	{
		Engine::instance()->rhi()->bindUniformBuffer(binding, *this);
	}

	void BufferHandle::bindStorage(uint32_t binding) const
	{
		Engine::instance()->rhi()->bindStorageBuffer(binding, *this);
	}

	void PipelineHandle::bind() const
	{
		Engine::instance()->rhi()->bindPipeline(*this);
	}
}
