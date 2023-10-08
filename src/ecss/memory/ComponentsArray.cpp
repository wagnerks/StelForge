#include "ComponentsArray.h"

#include "../base/ComponentBase.h"


namespace ecss::Memory {
	ComponentsArray::ComponentsArray(size_t capacity) : mCapacity(std::max(capacity, static_cast<size_t>(1))) {
		mChunkData.sectorMembersOffsets[0] = mChunkData.sectorSize += 0;
		mChunkData.sectorMembersOffsets[1] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(SectorInfo) + alignof(SectorInfo)); //offset for sector id
	}

	ComponentsArray::~ComponentsArray() {
		clear();

		mChunk->~SectorsChunk();
		std::free(mChunk);
	}

	size_t ComponentsArray::size() const {
		return mSize;
	}

	bool ComponentsArray::empty() const {
		return !size();
	}

	void ComponentsArray::clear() {
		for (EntityId i = 0; i < size(); i++) {
			destroySector((*mChunk)[i]);
		}

		mSize = 0;
		mChunk->size = 0;
		mSectorsMap.clear();
		mSectorsMap.resize(mCapacity, INVALID_ID);
	}

	void ComponentsArray::reserve(size_t newCapacity) {
		if (newCapacity <= mCapacity) {
			return;
		}

		setCapacity(newCapacity);
	}

	void ComponentsArray::shrinkToFit() {
		setCapacity(size());
	}

	void ComponentsArray::setCapacity(size_t newCap) {
		if (mCapacity == newCap) {
			return;
		}

		mCapacity = newCap;

		void* newMemory = malloc(sizeof(SectorsChunk) + alignof(SectorsChunk) + mCapacity * mChunkData.sectorSize);
		assert(newMemory);

		const auto newChunk = new(newMemory)SectorsChunk(std::move(*mChunk));
		std::free(mChunk);
		mChunk = newChunk;

		if (mCapacity > mSectorsMap.size()) {
			mSectorsMap.resize(mCapacity, INVALID_ID);
		}
	}

	void ComponentsArray::allocateChunk() {
		if (mChunk) {
			return;
		}

		mSectorsMap.resize(mCapacity, std::numeric_limits<uint32_t>::max());

		auto chunkPlace = malloc(sizeof(SectorsChunk) + alignof(SectorsChunk) + mChunkData.sectorSize * mCapacity);
		mChunk = new (chunkPlace)SectorsChunk(mChunkData);
	}

	void ComponentsArray::erase(size_t pos) {
		char* sectorPtr = static_cast<char*>((*mChunk)[pos]);

		mSectorsMap[static_cast<SectorInfo*>(static_cast<void*>(sectorPtr))->id] = std::numeric_limits<EntityId>::max();
		
		mChunk->shiftDataLeft(pos);
		--mSize;
		--mChunk->size;

		auto sectorIt = SectorsChunk::Iterator(sectorPtr, mChunkData);
		while (sectorIt != mChunk->end()) {
			mSectorsMap[static_cast<SectorInfo*>(*sectorIt)->id] = static_cast<EntityId>(pos++);
			++sectorIt;
		}
	}

	void* ComponentsArray::initSectorMember(void* sectorPtr, const ECSType componentTypeId) {
		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		const auto idx = mChunkData.sectorMembersIndexes[componentTypeId];
		destroyObject(sectorPtr, idx);

		sectorInfo->setTypeBitTrue(idx);
		return Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[idx]);
	}

	void* ComponentsArray::createSector(size_t pos, const EntityId sectorId) {
		char* sectorAdr = static_cast<char*>((*mChunk)[pos]);

		if (pos < size()) {
			mChunk->shiftDataRight(pos);
		}

		++mSize;
		++mChunk->size;
		const auto sectorInfo = new(sectorAdr)SectorInfo();;
		sectorInfo->id = sectorId;

		auto sectorIt = SectorsChunk::Iterator(sectorAdr, mChunkData);
		while (sectorIt != mChunk->end()) {
			auto id = static_cast<SectorInfo*>(*sectorIt)->id;
			mSectorsMap[id] = static_cast<EntityId>(pos++);
			++sectorIt;
		}

		return sectorAdr;
	}

	void* ComponentsArray::acquireSector(const ECSType componentTypeId, const EntityId entityId) {
		if (size() >= mCapacity) {
			setCapacity(mCapacity * 2);
		}

		if (mSectorsMap.size() <= entityId) {
			mSectorsMap.resize(entityId + 1, INVALID_ID);
		}

		if (mSectorsMap[entityId] < size()) {
			return initSectorMember((*mChunk)[mSectorsMap[entityId]], componentTypeId);
		}

		size_t idx = 0;
		Utils::binarySearch(entityId, idx, mChunk); //find the place where to insert sector

		return initSectorMember(createSector(idx, entityId), componentTypeId);
	}

	void ComponentsArray::destroyObject(const ECSType componentTypeId, const EntityId entityId) {
		const auto pos = mSectorsMap[entityId];
		if (pos >= size()) {
			return;
		}

		const auto sectorPtr = (*mChunk)[pos];
		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		destroyObject(sectorPtr, mChunkData.sectorMembersIndexes[componentTypeId]);
		
		if (sectorInfo->nullBits == 0) {
			erase(mSectorsMap[entityId]);
		}
	}

	void ComponentsArray::destroyObject(void* sectorPtr, uint8_t typeIdx) const {
		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		if (sectorInfo->isTypeNull(typeIdx)) {
			return;
		}
		sectorInfo->setTypeBitFalse(typeIdx);

		static_cast<ComponentInterface*>(Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[typeIdx]))->~ComponentInterface();
	}

	void ComponentsArray::destroySector(void* sectorPtr) const {
		for (auto& [typeId, typeIdx] : mChunk->data.sectorMembersIndexes) {
			destroyObject(sectorPtr, typeIdx);
		}
	}

	void ComponentsArray::destroySector(const EntityId entityId) {
		if (entityId >= mSectorsMap.size() || mSectorsMap[entityId] > mCapacity) {
			return;
		}

		destroySector((*mChunk)[mSectorsMap[entityId]]);

		erase(mSectorsMap[entityId]);
	}
}
