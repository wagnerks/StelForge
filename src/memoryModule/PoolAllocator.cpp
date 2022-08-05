#include "PoolAllocator.h"

#include <cmath>

#include "logsModule/logger.h"

using namespace GameEngine::MemoryModule;

PoolAllocator::PoolAllocator(size_t memSize, const void* mem, size_t chunkSize, uint8_t chunkAlignment) :
	Allocator(memSize, mem),
	chunkSize(chunkSize),
	chunkAlignment(chunkAlignment) {

	if (chunkSize < 8) {
		LogsModule::Logger::LOG_WARNING("Chunk size must be greater or equal to 8");
		chunkSize = 8;
	}
	if (const auto rem = totalSize % chunkSize; rem != 0) {
		LogsModule::Logger::LOG_WARNING("Total size must be a multiple of Chunk size");
		totalSize += chunkSize - rem;
	}

	PoolAllocator::reset();
}

PoolAllocator::~PoolAllocator() {
	PoolAllocator::free(const_cast<void*>(startAddress));
}

void* PoolAllocator::allocate(size_t memSize, uint8_t alignment) {
	if (memSize == 0) {
		return nullptr;
	}

	auto freePos = mFreeList.pop();
	if (!freePos) {
		LogsModule::Logger::LOG_FATAL(freePos, "The pool allocator is full");
		return nullptr;
	}

	memoryUsed += chunkSize;
	memoryAllocations++;

	return freePos;
}

void PoolAllocator::free(void* mem) {
	mFreeList.push(static_cast<Node*>(mem));

	memoryUsed -= chunkSize;
	memoryAllocations--;
}

void PoolAllocator::reset() {
	memoryUsed = 0;
	memoryAllocations = 0;

	const uint8_t adjustment = getAdjustment(startAddress, chunkAlignment);
	const size_t chunksCount = (totalSize - adjustment) / chunkSize;

	for (auto i = 0u; i < chunksCount; i++) {
		const size_t address = reinterpret_cast<size_t>(startAddress) + i * chunkSize;
		mFreeList.push(reinterpret_cast<Node*>(address));
	}
}