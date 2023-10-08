#include "Allocators.h"

#include <cassert>

namespace Engine::MemoryModule {

	void Allocator::init(size_t memSize, const void* mem) {
		mAllocationSize = memSize;
		mStartAddress = const_cast<void*>(mem);
	}

	size_t Allocator::getMemorySize() const {
		return mAllocationSize;
	}

	const void* Allocator::getStartAddress() const {
		return mStartAddress;
	}

	void* LinearAllocator::allocate(size_t memSize, uint8_t alignment) {
		const size_t memberSize = memSize + alignment;

		if (mOffset + memberSize > mAllocationSize) {
			assert(false);
			return nullptr;
		}

		const auto currentAddress = static_cast<char*>(mStartAddress) + mOffset;
		mOffset += memberSize;

		return static_cast<void*>(currentAddress);
	}

	void LinearAllocator::free(void* mem) {
		reset();
	}

	void LinearAllocator::reset() {
		mOffset = 0;
	}


	void* StackAllocator::allocate(size_t memSize, uint8_t alignment) {
		const size_t memberSize = memSize + alignment;

		if (mOffset + memberSize > mAllocationSize) {
			assert(false);
			return nullptr;
		}

		const auto currentAddress = static_cast<char*>(mStartAddress) + mOffset;
		mOffset += memberSize;

		return static_cast<void*>(currentAddress);
	}

	void StackAllocator::free(void* ptr) {
		const auto curAddress = reinterpret_cast<size_t>(ptr);
		mOffset = curAddress - reinterpret_cast<size_t>(mStartAddress);
	}

	void StackAllocator::reset() {
		mOffset = 0;
	}
}
