#pragma once
#include <cassert>

#include "MemoryManager.h"
#include "PoolAllocator.h"

namespace GameEngine::MemoryModule {

	class GlobalMemoryUser {
	protected:
		MemoryManager* globalMemoryManager = nullptr;

	public:
		GlobalMemoryUser(MemoryManager* memoryManager);
		virtual ~GlobalMemoryUser() = default;

		GlobalMemoryUser(const GlobalMemoryUser&) = delete;
		GlobalMemoryUser& operator=(GlobalMemoryUser&) = delete;

		const void* allocate(size_t memSize, size_t user = std::numeric_limits<size_t>::max()) const;
		void free(void* pMem) const;
	};

	template <class T, size_t CAPACITY>
	class MemoryChunkAllocator : protected GlobalMemoryUser {
		static const size_t ALLOC_SIZE = (sizeof(T) + alignof(T)) * CAPACITY;

		size_t allocatorID = 0;
		size_t capacity = CAPACITY;
	public:

		struct Chunk {
			PoolAllocator* allocator;
			std::list<T*> objects;

			uintptr_t begin;
			uintptr_t end;

			Chunk(PoolAllocator* allocator) : allocator(allocator) {
				this->begin = reinterpret_cast<uintptr_t>(allocator->getStartAddress());
				this->end = this->begin + ALLOC_SIZE;
			}
			~Chunk() {
				delete allocator;
			}
		};


		class iterator {
		public:
			typename std::list<Chunk*>::iterator currentChunkIt;
			typename std::list<Chunk*>::iterator chunksEndIt;

			typename std::list<T*>::iterator currentObjIt;
		
			friend bool operator==(const iterator& lhs, const iterator& rhs) {
				return lhs.currentChunkIt == rhs.currentChunkIt && lhs.currentObjIt == rhs.currentObjIt;
			}

			friend bool operator!=(const iterator& lhs, const iterator& rhs) {
				return !(lhs == rhs);
			}


			iterator(typename std::list<Chunk*>::iterator begin, typename std::list<Chunk*>::iterator end) : currentChunkIt(begin), chunksEndIt(end) {
				if (begin != end) {
					assert((*currentChunkIt) != nullptr);
					currentObjIt = (*currentChunkIt)->objects.begin();
				}
				else {
					currentObjIt = (*std::prev(chunksEndIt))->objects.end();
				}
			}


			inline const iterator& operator++() {
				++currentObjIt;

				if (currentObjIt == (*currentChunkIt)->objects.end()) {
					++currentChunkIt;

					if (currentChunkIt != chunksEndIt) {
						assert((*currentChunkIt) != nullptr);
						currentObjIt = (*currentChunkIt)->objects.begin();
					}
				}

				return *this;
			}

			inline const iterator& operator++(int) {
				iterator tmp = *this;
				++(*this);
				return tmp;
			}

			inline T& operator*() const {
				return **currentObjIt;
			}

			inline T* operator->() const {
				return *currentObjIt;
			}
		};


		MemoryChunkAllocator(size_t id, MemoryManager* memoryManager);
		~MemoryChunkAllocator() override;

		void* createObject();
		void destroyObject(void* object);

		MemoryChunkAllocator(const MemoryChunkAllocator&) = delete;
		MemoryChunkAllocator& operator=(MemoryChunkAllocator&) = delete;

		inline iterator begin() {
			return iterator(this->chunksBlock.begin(), this->chunksBlock.end());
		}
		inline iterator end() {
			return iterator(this->chunksBlock.end(), this->chunksBlock.end());
		}
		inline iterator begin() const {
			return iterator(this->chunksBlock.begin(), this->chunksBlock.end());
		}
		inline iterator end() const {
			return iterator(this->chunksBlock.end(), this->chunksBlock.end());
		}

	protected:
		std::list<Chunk*> chunksBlock;
	};

	template <class T, size_t CAPACITY>
	MemoryChunkAllocator<T, CAPACITY>::MemoryChunkAllocator(size_t id, MemoryManager* memoryManager) : GlobalMemoryUser(memoryManager), allocatorID(id)  {
		auto firstAllocator = new PoolAllocator(ALLOC_SIZE, allocate(ALLOC_SIZE, allocatorID), sizeof(T), alignof(T));
		this->chunksBlock.push_back(new Chunk(firstAllocator));
	}

	template <class T, size_t CAPACITY>
	MemoryChunkAllocator<T, CAPACITY>::~MemoryChunkAllocator() {
		for (auto chunk : this->chunksBlock) {
			for (auto obj : chunk->objects) {
				obj->~T();
			}

			globalMemoryManager->free((void*)chunk->begin);
			delete chunk;
		}
	}

	template <class T, size_t CAPACITY>
	void* MemoryChunkAllocator<T, CAPACITY>::createObject() {
		void* slot = nullptr;

		// get next free slot
		for (auto chunk : this->chunksBlock) {
			if (chunk->objects.size() > CAPACITY) {
				continue;
			}
					

			slot = chunk->allocator->allocate(sizeof(T), alignof(T));
			if (slot != nullptr) {
				chunk->objects.push_back(static_cast<T*>(slot));
				break;
			}
		}

		// all chunks are full... allocate a new one
		if (slot == nullptr) {
			auto memoryPlace = allocate(ALLOC_SIZE, this->allocatorID);
			if (!memoryPlace) {
				assert("Unable to create new object. Out of memory?!");
			}

			auto newChunk = new Chunk(new PoolAllocator(ALLOC_SIZE, memoryPlace, sizeof(T), alignof(T)));
			this->chunksBlock.push_front(newChunk);

			slot = newChunk->allocator->allocate(sizeof(T), alignof(T));
			assert(slot != nullptr && "Unable to create new object. Out of memory?!");

			newChunk->objects.push_back(static_cast<T*>(slot));
		}

		return slot;
	}

	template <class T, size_t CAPACITY>
	void MemoryChunkAllocator<T, CAPACITY>::destroyObject(void* object) {
		auto adr = reinterpret_cast<uintptr_t>(object);

		for (auto chunk : this->chunksBlock) {
			if (chunk->begin <= adr && adr < chunk->end) {
				// note: no need to call d'tor since it was called already by 'delete'

				chunk->objects.remove(static_cast<T*>(object));
				chunk->allocator->free(object);
				return;
			}
		}

		assert(false && "Failed to delete object. Memory corruption?!");
	}
}
