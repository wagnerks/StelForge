#include "rhi/NullRHI.h"

namespace SFE::Render {
	BufferHandle NullRHI::createBuffer(const BufferDesc& desc, const void*) {
		const BufferHandle handle{ nextId() };
		std::lock_guard lock(mBuffersMutex);
		mBufferSizes[handle.id] = desc.size;
		return handle;
	}

	void NullRHI::destroyBuffer(const BufferHandle handle) {
		std::lock_guard lock(mBuffersMutex);
		mBufferSizes.erase(handle.id);
		mMapped.erase(handle.id);
	}

	void* NullRHI::mapBuffer(const BufferHandle handle, MapMode) {
		std::lock_guard lock(mBuffersMutex);
		const auto size = mBufferSizes.find(handle.id);
		if (size == mBufferSizes.end()) {
			return nullptr;
		}
		auto& memory = mMapped[handle.id];
		memory.resize(size->second);
		return memory.data();
	}

	TransientAllocation NullRHI::allocTransient(const size_t size) {
		auto& memory = mTransient.emplace_back(size == 0 ? 1 : size);
		return { mTransientBuffer, 0, memory.data() };
	}

	void NullRHI::beginFrame() {
		mTransient.clear();
	}
}
