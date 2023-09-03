#pragma once

#include <cstdint>

namespace Engine::MemoryModule {
	static inline uint8_t getAdjustment(const void* address, uint8_t alignment) {
		const uint8_t adjustment = alignment - (reinterpret_cast<uintptr_t>(address)& static_cast<uintptr_t>(alignment - 1));
	
		return adjustment == alignment ? 0 : adjustment;
	}
	
	static inline uint8_t getAdjustment(const void* address, uint8_t alignment, uint8_t extra) {
		uint8_t adjustment = getAdjustment(address, alignment);
	
		uint8_t neededSpace = extra;
	
		if (adjustment < neededSpace) {
			neededSpace -= adjustment;
	
			//Increase adjustment to fit header
			adjustment += alignment * (neededSpace / alignment);
	
			if (neededSpace % alignment > 0) {
				adjustment += alignment;
			}
		}
	
		return adjustment;
	}

	template<class T>
	class StackLinkedList {
	public:
		struct Node {
			T data;
			Node* next;
		};

		Node* head;
	public:
		StackLinkedList() = default;
		StackLinkedList(StackLinkedList &a) = delete;

		void push(Node* newNode);
		Node* pop();
	};

	template <class T>
	void StackLinkedList<T>::push(Node* newNode) {
		newNode->next = head;
		head = newNode;
	}

	template <class T>
	typename StackLinkedList<T>::Node* StackLinkedList<T>::pop() {
		Node* top = head;
		head = head->next;
		return top;
	}

	class Allocator {
	public:
	
		Allocator(const size_t memSize, const void* mem)
		: totalSize(memSize)
		, startAddress(mem)
		, memoryUsed(0)
		, memoryAllocations(0) {}

		virtual ~Allocator() = default;
	
		virtual void* allocate(size_t size, uint8_t alignment) = 0;
		virtual void free(void* p) = 0;
		virtual void reset() = 0;
	
		size_t getMemorySize() const;
		const void* getStartAddress() const;
		size_t getUsedMemory() const;
		uint64_t getAllocationCount() const;
	protected:
		size_t totalSize;
		const void*	 startAddress;

		size_t		 memoryUsed;
		uint64_t	 memoryAllocations;
	};
}