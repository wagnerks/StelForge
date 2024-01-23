#pragma once
#include <limits>

#include "MemoryManager.h"

namespace SFE::MemoryModule {

	class GlobalMemoryUser {
		GlobalMemoryUser(const GlobalMemoryUser&) = delete;
		GlobalMemoryUser& operator=(GlobalMemoryUser&) = delete;
	protected:
		MemoryManager* mGlobalMemoryManager = nullptr;

	public:
		GlobalMemoryUser(MemoryManager* memoryManager);
		virtual ~GlobalMemoryUser() = default;

		const void* allocate(size_t memSize, size_t user = std::numeric_limits<size_t>::max()) const;
		void free(void* pMem) const;
	};
}
