#include "MemoryManager.h"

#include <cassert>

using namespace GameEngine::MemoryModule;

size_t MemoryManager::getMemoryCapacity() const {
	return mMemoryCapacity;
}

MemoryManager::MemoryManager(size_t memoryCapacity) : mMemoryCapacity(memoryCapacity), globalMemoryAddress(nullptr), allocator(nullptr) {
	globalMemoryAddress = malloc(mMemoryCapacity);

	if (!globalMemoryAddress) {
		LogsModule::Logger::LOG_FATAL(globalMemoryAddress, "Failed to allocate %d bytes of memory!", mMemoryCapacity);
		return;
	}
	LogsModule::Logger::LOG_INFO("%d bytes of memory allocated.", mMemoryCapacity);

	allocator = new StackAllocator(mMemoryCapacity, globalMemoryAddress);
	LogsModule::Logger::LOG_FATAL(allocator, "Failed to create memory allocator!");
}

MemoryManager::~MemoryManager() {
	checkMemoryLeaks();
	delete allocator;
}

void MemoryManager::checkMemoryLeaks() {
	if (!pendingMemory.empty()) {
		LogsModule::Logger::LOG_FATAL(false, "!!!  M E M O R Y   L E A K   D E T E C T E D  !!!");

		for (auto& i : pendingMemory) {
			auto it = std::ranges::find_if(freedMemory, [i](const void* a) {
				return i.second == a;
			});
			LogsModule::Logger::LOG_FATAL(it != freedMemory.end(), "\'%s\' memory user didn't release allocated memory %p!", i.first, i.second);
		}
	}
	else {
		LogsModule::Logger::LOG_INFO("No memory leaks detected.");
	}
}
