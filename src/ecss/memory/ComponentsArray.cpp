#include "ComponentsArray.h"

#include "../base/ComponentBase.h"


namespace ecss::Memory {
	ComponentsArray::ComponentsArray(size_t capacity) : mCapacity(std::max(capacity, static_cast<size_t>(1))) {
		mChunkData.sectorMembersOffsets[0] = mChunkData.sectorSize += 0;
		mChunkData.sectorMembersOffsets[1] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(SectorInfo) + alignof(SectorInfo)); //offset for sector id
	}

	ComponentsArray::~ComponentsArray() {
		if (!mChunk) {
			return;
		}

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
		if (!mChunk) {
			return;
		}

		for (EntityId i = 0; i < size(); i++) {
			destroySector((*mChunk)[i]);
		}

		mSize = 0;
		mChunk->size = 0;
		mSectorsMap.clear();
		mSectorsMap.resize(mCapacity, nullptr);
	}

	size_t ComponentsArray::capacity() const {
		return mCapacity;
	}

	size_t ComponentsArray::entitiesCapacity() const {
		return mSectorsMap.size();
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
			mSectorsMap.resize(mCapacity, nullptr);
		}

		for (auto sectorPtr : *mChunk) {
			mSectorsMap[static_cast<SectorInfo*>(static_cast<void*>(sectorPtr))->id] = sectorPtr;
		}
	}

	void ComponentsArray::allocateChunk() {
		if (mChunk) {
			return;
		}

		mSectorsMap.resize(mCapacity, nullptr);

		auto chunkPlace = malloc(sizeof(SectorsChunk) + alignof(SectorsChunk) + mChunkData.sectorSize * mCapacity);
		mChunk = new (chunkPlace)SectorsChunk(mChunkData);
	}

	void ComponentsArray::erase(size_t pos) {
		char* sectorPtr = static_cast<char*>((*mChunk)[pos]);
		auto info = static_cast<SectorInfo*>(static_cast<void*>(sectorPtr));
		mSectorsMap[info->id] = nullptr;
		
		mChunk->shiftDataLeft(pos);
		--mSize;
		--mChunk->size;

		auto sectorIt = SectorsChunk::Iterator(sectorPtr, mChunkData);
		while (sectorIt != mChunk->end()) {
			mSectorsMap[static_cast<SectorInfo*>(*sectorIt)->id] = *sectorIt;
			++sectorIt;
		}
	}

	void* ComponentsArray::initSectorMember(void* sectorPtr, const uint8_t componentTypeIdx) {
		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		destroyObject(sectorPtr, componentTypeIdx);

		sectorInfo->setTypeBitTrue(componentTypeIdx);
		return Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[componentTypeIdx]);
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
			mSectorsMap[id] = *sectorIt;
			++sectorIt;
		}

		return sectorAdr;
	}

	void* ComponentsArray::acquireSector(const uint8_t componentTypeIdx, const EntityId entityId) {
		if (size() >= mCapacity) {
			setCapacity(mCapacity * 2);
		}

		if (mSectorsMap.size() <= entityId) {
			mSectorsMap.resize(entityId + 1, nullptr);
		}

		if (const auto sectorPtr = mSectorsMap[entityId]) {
			return initSectorMember(sectorPtr, componentTypeIdx);
		}

		size_t idx = 0;
		Utils::binarySearch(entityId, idx, mChunk); //find the place where to insert sector

		return initSectorMember(createSector(idx, entityId), componentTypeIdx);
	}

	void ComponentsArray::destroyObject(const ECSType componentTypeId, const EntityId entityId) {
		const auto sectorPtr = mSectorsMap[entityId];
		if (!sectorPtr) {
			return;
		}

		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		destroyObject(sectorPtr, mChunkData.sectorMembersIndexes[componentTypeId]);
		auto dead = true;
		for (auto bit : sectorInfo->nullBits) {
			if (bit) {
				dead = false;
				break;
			}
		}

		if (dead) {
			size_t pos = 0;
			Utils::binarySearch(entityId, pos, mChunk);
			erase(pos);
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
		if (entityId >= mSectorsMap.size() || !mSectorsMap[entityId]) {
			return;
		}

		destroySector(mSectorsMap[entityId]);

		size_t pos = 0;
		Utils::binarySearch(entityId, pos, mChunk);
		erase(pos);
	}
}
