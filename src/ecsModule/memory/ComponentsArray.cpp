#include "ComponentsArray.h"

#include <cassert>

#include "ECSMemoryStack.h"
#include "../base/ComponentBase.h"

namespace ECS::Memory {
	namespace Utils {
		void* getTypePlace(void* start, ECSType typeId, const std::array<uint16_t, 34>& offsets, const std::unordered_map<ECSType, uint8_t, NoHash<ECSType>>& types) {
			return getTypePlace(start, offsets[types.at(typeId)]);
		}

		void* getTypePlace(void* start, uint16_t offset) {
			return static_cast<void*>(static_cast<char*>(start) + offset);
		}

		size_t distance(void* beg, void* end, size_t size) {
			return std::abs((static_cast<char*>(beg) - static_cast<char*>(end))) / size;
		}

		void* binarySearch(EntityId sectorId, size_t& idx, SectorsChunk* sectors) {
			auto begin = sectors->begin();
			auto end = sectors->end();
			if (begin == end) {
				idx = 0;
				return nullptr;
			}

			if (static_cast<SectorInfo*>((static_cast<void*>(static_cast<char*>(*begin) + sectors->size)))->id < sectorId) {
				idx = sectors->size;
				return nullptr;
			}

			auto it = begin;

			while (true) {
				it = begin;

				const auto dist = Utils::distance(*begin, *end, sectors->data.sectorSize);
				if (dist == 1) {
					idx = Utils::distance(*sectors->begin(), *it, sectors->data.sectorSize) + 1;
					break;
				}

				it = it + dist / 2;

				if (static_cast<SectorInfo*>(*it)->id > sectorId) {
					end = it;
				}
				else if (static_cast<SectorInfo*>(*it)->id == sectorId) {
					idx = Utils::distance(*sectors->begin(), *it, sectors->data.sectorSize);
					return *it;
				}
				else {
					begin = it;
				}
			}

			return nullptr;

		}
	}

	SectorsChunk::SectorsChunk(void* start, const ChunkData& data)
		: data(data), beginAdr(start) {}

	void* SectorsChunk::operator[](size_t i) {
		if (i >= size) {
			return nullptr;
		}

		return static_cast<char*>(beginAdr) + i * data.sectorSize;
	}

	void* SectorsChunk::Iterator::operator*() const {
		return ptr;
	}

	SectorsChunk::Iterator& SectorsChunk::Iterator::operator++() {
		ptr = static_cast<char*>(ptr) + sectorSize;
		return *this;
	}

	SectorsChunk::Iterator& SectorsChunk::Iterator::operator+(size_t i) {
		ptr = static_cast<char*>(ptr) + i * sectorSize;
		return *this;
	}

	bool SectorsChunk::Iterator::operator!=(const Iterator& other) const {
		return ptr != other.ptr;
	}

	SectorsChunk::Iterator& SectorsChunk::Iterator::operator=(const Iterator& other) {
		if (this != &other) {
			ptr = other.ptr;
			sectorSize = other.sectorSize;
		}
		return *this;
	}

	bool SectorsChunk::Iterator::operator==(const Iterator& other) const {
		return ptr == other.ptr;
	}

	SectorsChunk::Iterator SectorsChunk::begin() {
		return Iterator(beginAdr, data);
	}

	SectorsChunk::Iterator SectorsChunk::end() {
		return Iterator(static_cast<char*>(beginAdr) + size * data.sectorSize, data);
	}


	ComponentsArray::ComponentsArray(size_t capacity, ECS::Memory::ECSMemoryStack* memoryManager)
		: ECSMemoryUser(memoryManager)
	{
		mChunkData.chunkCapacity = capacity;

		mChunkData.sectorMembersOffsets[0] = mChunkData.sectorSize += 0;
		mChunkData.sectorMembersOffsets[1] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(SectorInfo) + alignof(SectorInfo)); //offset for sector id
	}

	ComponentsArray::~ComponentsArray() {
		clearAllSectors();

		mChunk->~SectorsChunk();
	}

	void ComponentsArray::clearAllSectors() {
		for (const auto sectorPtr : *mChunk) {
			destroySector(static_cast<SectorInfo*>(sectorPtr)->id, false);
		}
	}

	size_t ComponentsArray::size() const {
		return mChunk->size;
	}

	bool ComponentsArray::empty() const {
		return !size();
	}

	void ComponentsArray::allocateNewChunk() {
		if (mChunk) {
			return;
		}

		mSectorsMap.resize(mChunkData.chunkCapacity, std::numeric_limits<EntityId>::max());

		auto chunkBegin = allocate(mChunkData.chunkAllocationSize + sizeof(SectorsChunk) + alignof(SectorsChunk));
		assert(chunkBegin && "Unable to create new object. Out of memory?!");

		const auto sectorsBegin = static_cast<char*>(chunkBegin) + sizeof(SectorsChunk) + alignof(SectorsChunk);
		mChunk = new (chunkBegin)SectorsChunk(sectorsBegin, mChunkData);
	}

	void* ComponentsArray::createSector(size_t pos, const size_t size, const EntityId sectorId) {
		if (pos >= mChunk->data.chunkCapacity) {
			return nullptr;
		}

		const auto sectorPtr = static_cast<char*>(mChunk->beginAdr) + pos * size;

		if (pos >= mChunk->size) {
			++mChunk->size; //just increase size
		}
		else {
			//move data one sector right
			memmove(static_cast<void*>(sectorPtr + size), sectorPtr, (mChunk->size - pos) * size);
			++mChunk->size;
		}

		const auto sectorInfo = static_cast<SectorInfo*>(static_cast<void*>(sectorPtr));
		sectorInfo->id = sectorId;
		sectorInfo->nullBits = 0;

		auto sectorIt = SectorsChunk::Iterator(sectorPtr, mChunkData);
		while (sectorIt != mChunk->end()) {
			mSectorsMap[static_cast<SectorInfo*>(*sectorIt)->id] = static_cast<EntityId>(pos++);
			++sectorIt;
		}

		return sectorPtr;
	}

	void ComponentsArray::erase(const size_t pos, const size_t size) {
		if (pos >= mChunk->size) {
			return;
		}

		const auto sectorPtr = static_cast<char*>(mChunk->beginAdr) + pos * size;

		mSectorsMap[static_cast<SectorInfo*>(static_cast<void*>(sectorPtr))->id] = std::numeric_limits<EntityId>::max();

		memmove(static_cast<void*>(sectorPtr), sectorPtr + size, (mChunk->size - pos) * size);
		--mChunk->size;
	}

	void* ComponentsArray::initSectorMember(void* sectorPtr, const ECSType componentTypeId) {
		auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		sectorInfo->setTypeBitTrue(mChunkData.sectorMembersIndexes[componentTypeId]);
		return Utils::getTypePlace(sectorPtr, componentTypeId, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes);
	}

	void* ComponentsArray::acquireSector(const ECSType componentTypeId, const EntityId entityId) {
		if (entityId >= mChunkData.chunkCapacity) {
			assert(false && "can't acquire sector");
			return nullptr;
		}

		if (const auto sectorPtr = mChunk->operator[](mSectorsMap[entityId])) {
			return initSectorMember(sectorPtr, componentTypeId);
		}

		size_t idx = 0;
		Utils::binarySearch(entityId, idx, mChunk); //find the place where to insert sector

		const auto sectorPtr = createSector(idx, mChunkData.sectorSize, entityId);
		assert(sectorPtr);

		return initSectorMember(sectorPtr, componentTypeId);
	}

	void ComponentsArray::destroyObject(const ECSType componentTypeId, const EntityId entityId, bool _erase) {
		const auto sectorPtr = (*mChunk)[mSectorsMap[entityId]];
		if (!sectorPtr) {
			return;
		}

		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		const auto memberIdx = mChunkData.sectorMembersIndexes[componentTypeId];
		if (sectorInfo->isTypeNull(memberIdx)) {
			return;
		}
		sectorInfo->setTypeBitFalse(memberIdx);

		static_cast<ECS::ComponentInterface*>(Utils::getTypePlace(sectorPtr, componentTypeId, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes))->~ComponentInterface();

		if (_erase) {
			if (sectorInfo->nullBits == 0) {
				erase(mSectorsMap[entityId], mChunkData.sectorSize);
			}
		}
	}

	void ComponentsArray::destroySector(const EntityId entityId, bool _erase) {
		if (entityId >= mChunkData.chunkCapacity) {
			return;
		}

		const auto sectorPtr = (*mChunk)[mSectorsMap[entityId]];
		if (!sectorPtr) {
			return;
		}

		for (auto& [typeId, typeIdx] : mChunk->data.sectorMembersIndexes) {
			destroyObject(typeId, entityId, _erase);
		}

		if (_erase) {
			erase(mSectorsMap[entityId], mChunkData.sectorSize);
		}
	}
}
