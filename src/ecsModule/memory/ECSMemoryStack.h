#pragma once
#include <queue>

#include "Allocators.h"

namespace ECS::Memory {
	class ECSMemoryStack;

	class ECSMemoryUser {
		ECSMemoryUser(const ECSMemoryUser&) = delete;
		ECSMemoryUser& operator=(ECSMemoryUser&) = delete;

	protected:
		ECSMemoryStack* mStack = nullptr;

	public:
		ECSMemoryUser(ECSMemoryStack* ecsMemoryStack);
		virtual ~ECSMemoryUser() = default;

		void* allocate(size_t memSize) const;
		void free(void* pMem) const;
	};

	class ECSMemoryStack final {
		ECSMemoryStack(const ECSMemoryStack&) = delete;
		ECSMemoryStack& operator=(ECSMemoryStack&) = delete;

	public:
		ECSMemoryStack(size_t memoryCapacity);
		~ECSMemoryStack();

		void* allocate(size_t memSize);
		void free(void* pMem);

	private:
		const size_t		mMemoryCapacity;
		const void*			mGlobalAddress;

		StackAllocator		mAllocator;
		std::deque<void*>	mPendingMemory;
	};

}
