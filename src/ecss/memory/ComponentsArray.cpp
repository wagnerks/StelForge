#include "ComponentsArray.h"

#include "../base/ComponentBase.h"


namespace ecss::Memory {
	ComponentsArray::ComponentsArray(size_t capacity) {
		mChunkData.sectorMembersOffsets[0] = mChunkData.sectorSize += 0;
		mChunkData.sectorMembersOffsets[1] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(SectorInfo) + alignof(SectorInfo)); //offset for sector id
	}

	ComponentsArray::~ComponentsArray() {
		if (!mData) {
			return;
		}

		clear();

		std::free(mData);
	}

	size_t ComponentsArray::size() const {
		return mSize;
	}

	bool ComponentsArray::empty() const {
		return !size();
	}

	void ComponentsArray::clear() {
		if (!mSize) {
			return;
		}

		for (EntityId i = 0; i < size(); i++) {
			destroySector((*this)[i]);
		}

		mSize = 0;
		mSectorsMap.clear();
		mSectorsMap.resize(mCapacity, INVALID_ID);
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

		if (!mData) {
			mData = malloc(mChunkData.sectorSize * mCapacity);
			assert(mData);
		}
		else {
			void* newMemory = malloc(mCapacity * mChunkData.sectorSize);
			assert(newMemory);

			for (size_t i = 0; i < size(); i++) {
				const auto sectorPtr = static_cast<void*>(static_cast<char*>(mData) + i * mChunkData.sectorSize);
				const auto copySector = static_cast<void*>(static_cast<char*>(newMemory) + i * mChunkData.sectorSize);

				const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
				const auto newSectorInfo = static_cast<SectorInfo*>(copySector);

				new(copySector)SectorInfo(std::move(*sectorInfo));

				for (auto [typeId, typeIdx] : mChunkData.sectorMembersIndexes) {
					if (!sectorInfo->isAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx])) {
						newSectorInfo->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx], false);
						continue;
					}

					const auto oldPlace = Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[typeIdx]);
					const auto newPlace = Utils::getTypePlace(copySector, mChunkData.sectorMembersOffsets[typeIdx]);
					ReflectionHelper::moveMap[typeId](newPlace, oldPlace);//call move constructor
					newSectorInfo->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx], true);
				}
			}

			std::free(mData);
			mData = newMemory;
		}

		if (mCapacity > mSectorsMap.size()) {
			mSectorsMap.resize(mCapacity, INVALID_ID);
		}
	}

	void ComponentsArray::erase(size_t pos) {
		const auto sectorInfo = (*this)[pos];
		mSectorsMap[sectorInfo->id] = INVALID_ID;

		shiftDataLeft(pos);
		--mSize;
	}

	void ComponentsArray::erase(size_t from, size_t to) {
		shiftDataLeft(from, to - from);
		mSize -= to - from;
	}

	void* ComponentsArray::initSectorMember(void* sectorPtr, const uint8_t componentTypeIdx) const {
		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		destroyObject(sectorPtr, componentTypeIdx);

		sectorInfo->setAlive(mChunkData.sectorMembersOffsets[componentTypeIdx], mChunkData.sectorMembersOffsets[componentTypeIdx + 1] - mChunkData.sectorMembersOffsets[componentTypeIdx], true);
		return Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[componentTypeIdx]);
	}

	void* ComponentsArray::createSector(size_t pos, const EntityId sectorId) {
		const auto sectorAdr = (*this)[pos];

		if (pos < size()) {
			shiftDataRight(pos);
		}

		++mSize;
		sectorAdr->id = sectorId;
		for (auto i = 1u; i < 1 + mSectorCapacity; i++) {//1 is reserved for sector info
			sectorAdr->setAlive(mChunkData.sectorMembersOffsets[i], mChunkData.sectorMembersOffsets[i + 1] - mChunkData.sectorMembersOffsets[i], false);
		}

		mSectorsMap[sectorAdr->id] = static_cast<EntityId>(pos);

		return sectorAdr;
	}

	void* ComponentsArray::acquireSector(const uint8_t componentTypeIdx, const EntityId entityId) {
		if (size() >= mCapacity) {
			if (!mCapacity) {
				mCapacity = 1;
			}
			setCapacity(mCapacity * 2);
		}

		if (mSectorsMap.size() <= entityId) {
			mSectorsMap.resize(entityId + 1, INVALID_ID);
		}
		else {
			if (mSectorsMap[entityId] < size()) {
				return initSectorMember((*this)[mSectorsMap[entityId]], componentTypeIdx);
			}
		}

		size_t idx = 0;
		Utils::binarySearch(entityId, idx, this, mChunkData.sectorSize); //find the place where to insert sector

		return initSectorMember(createSector(idx, entityId), componentTypeIdx);
	}

	void ComponentsArray::destroyObject(const ECSType componentTypeId, const EntityId entityId) {
		if (mSectorsMap[entityId] >= size()) {
			return;
		}

		const auto sectorInfo = (*this)[mSectorsMap[entityId]];

		destroyObject(sectorInfo, mChunkData.sectorMembersIndexes[componentTypeId]);

		if (!isSectorAlive(sectorInfo)) {
			size_t pos = 0;
			Utils::binarySearch(entityId, pos, this, mChunkData.sectorSize);
			erase(pos);
		}
	}

	void ComponentsArray::destroyObjects(ECSType componentTypeId, std::vector<EntityId> entityIds) {
		if (entityIds.empty()) {
			return;
		}

		std::sort(entityIds.begin(), entityIds.end());
		if (entityIds.front() >= size()) {
			return;
		}

		auto prevPos = mSectorsMap[entityIds.front()];
		auto lastPos = mSectorsMap[entityIds.front()];

		for (auto i = 0u; i < entityIds.size(); i++) {
			const auto entityId = entityIds[i];
			if (entityId == INVALID_ID) {
				break; //all valid entities destroyed
			}

			if (mSectorsMap[entityId] >= size()) {
				continue;//there is no such entity in container
			}

			const auto sector = (*this)[mSectorsMap[entityId]];
			destroyObject(sector, mChunkData.sectorMembersIndexes[componentTypeId]);

			if (!isSectorAlive(sector)) {
				lastPos = mSectorsMap[entityId];
				mSectorsMap[entityId] = INVALID_ID;

				const bool isLast = i == entityIds.size() - 1;
				if (isLast || ((entityIds[i + 1] - entityId) > 1)) {
					erase(prevPos, lastPos);
					prevPos = lastPos;
				}
				//continue iterations till not found some nod dead sector
				continue;
			}

			erase(prevPos, lastPos);//todo not sure this part works correctly
			prevPos = lastPos;
		}
	}

	void ComponentsArray::destroyObject(void* sectorPtr, uint8_t typeIdx) const {
		const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);
		if (!sectorInfo->isAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx])) {
			return;
		}

		sectorInfo->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx],false);
		for (auto [typeId, idx] : mChunkData.sectorMembersIndexes) {
			if (idx == typeIdx) {
				ReflectionHelper::destructorMap[typeId](Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[typeIdx]));
				break;
			}
		}
	}

	void ComponentsArray::destroySector(void* sectorPtr) const {
		for (auto [typeId, typeIdx] : mChunkData.sectorMembersIndexes) {
			destroyObject(sectorPtr, typeIdx);
		}
	}

	void ComponentsArray::destroySector(const EntityId entityId) {
		if (entityId >= mSectorsMap.size() || mSectorsMap[entityId] >= size()) {
			return;
		}

		destroySector((*this)[mSectorsMap[entityId]]);

		size_t pos = 0;
		Utils::binarySearch(entityId, pos, this, mChunkData.sectorSize);
		erase(pos);
	}

	ComponentsArray::IteratorSectors ComponentsArray::beginSectors() const {
		return IteratorSectors(mData, mChunkData);
	}

	ComponentsArray::IteratorSectors ComponentsArray::endSectors() const {
		return IteratorSectors((*this)[size()], mChunkData);
	}

	void ComponentsArray::shiftDataRight(size_t from) {
		for (auto i = size() - 1; i >= from; i--) {
			auto prevAdr = (*this)[i];
			auto newAdr = (*this)[i + 1];

			for (auto [typeId, typeIdx] : mChunkData.sectorMembersIndexes) {
				if (!prevAdr->isAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx])) {
					newAdr->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx], false);
					continue;
				}

				const auto oldPlace = Utils::getTypePlace(prevAdr, mChunkData.sectorMembersOffsets[typeIdx]);
				const auto newPlace = Utils::getTypePlace(newAdr, mChunkData.sectorMembersOffsets[typeIdx]);
				ReflectionHelper::moveMap[typeId](newPlace, oldPlace);//call move constructor

				newAdr->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx], true);
			}

			//move data one sector right to empty place for new sector
			new (newAdr)SectorInfo(std::move(*prevAdr));//call move constructor for sector info
			mSectorsMap[newAdr->id] = static_cast<EntityId>(i + 1);

			if (i == 0) {
				break;
			}
		}
	}

	void ComponentsArray::shiftDataLeft(size_t from, size_t offset) {
		for (auto i = from; i < size() - offset; i++) {
			auto newAdr = (*this)[i];
			auto prevAdr = (*this)[i + offset];

			for (auto [typeId, typeIdx] : mChunkData.sectorMembersIndexes) {
				if (!prevAdr->isAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx])) {
					newAdr->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx], false);
					continue;
				}

				const auto oldPlace = Utils::getTypePlace(prevAdr, mChunkData.sectorMembersOffsets[typeIdx]);
				const auto newPlace = Utils::getTypePlace(newAdr, mChunkData.sectorMembersOffsets[typeIdx]);
				ReflectionHelper::moveMap[typeId](newPlace, oldPlace);//call move constructor
				newAdr->setAlive(mChunkData.sectorMembersOffsets[typeIdx], mChunkData.sectorMembersOffsets[typeIdx + 1] - mChunkData.sectorMembersOffsets[typeIdx], true);
			}

			new (newAdr)SectorInfo(std::move(*prevAdr));//move sector info
			mSectorsMap[newAdr->id] = static_cast<EntityId>(i);
		}
	}

	bool ComponentsArray::isSectorAlive(SectorInfo* sector) const {
		bool alive = true;
		for (auto i = 1u; i < 1 + mSectorCapacity; i++) {
			if (!sector->isAlive(mChunkData.sectorMembersOffsets[i], mChunkData.sectorMembersOffsets[i + 1] - mChunkData.sectorMembersOffsets[i])) {
				alive = false;
				break;
			}
		}

		return alive;
	}
}
