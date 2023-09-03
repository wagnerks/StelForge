#pragma once

#include <list>
#include <vector>

#include "StackAllocator.h"
#include "logsModule/logger.h"

namespace Engine::MemoryModule {

	class StackAllocator;

	class MemoryManager {
		friend class GlobalMemoryUser;
	private:
		const size_t mMemoryCapacity;

		void* globalMemoryAddress;

		StackAllocator* allocator;

		std::vector<std::pair<size_t, void*>> pendingMemory;

		std::list<void*> freedMemory;
	public:
		size_t getMemoryCapacity() const;;


		MemoryManager(const MemoryManager&) = delete;
		MemoryManager& operator=(MemoryManager&) = delete;

		MemoryManager(size_t memoryCapacity);
		~MemoryManager();

		inline void* allocate(size_t memSize, size_t user) {
			LogsModule::Logger::LOG_INFO("%zu allocated %d bytes of global memory.", user, memSize);

			void* pMemory = allocator->allocate(memSize, alignof(uint8_t));
			pendingMemory.emplace_back(user, pMemory);

			return pMemory;
		}

		inline void free(void* pMem) {
			if (pMem == pendingMemory.back().second) {
				allocator->free(pMem);
				pendingMemory.pop_back();


				for (auto it = freedMemory.begin(); it != freedMemory.end(); ) {
					if (*it == pendingMemory.back().second) {
						allocator->free(*it);
						pendingMemory.pop_back();
						freedMemory.erase(it);
						it = freedMemory.begin();
					}
					else {
						++it;
					}
				}
			}
			else {
				freedMemory.push_back(pMem);
			}
		}

		void checkMemoryLeaks();
	};
}
