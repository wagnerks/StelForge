#include "ComponentsArraySector.h"

#include "../base/ComponentBase.h"


namespace ecss::Memory {
	SectorsChunk::SectorsChunk(SectorsChunk&& other) noexcept
		: size(std::move(other.size))
		, data(std::move(other.data))
		, beginAdr(static_cast<char*>(static_cast<void*>(this)) + sizeof(SectorsChunk) + alignof(SectorsChunk)) {

		for (size_t i = 0; i < size; i++) {
			const auto sectorPtr = (other)[i];
			const auto copySector = (*this)[i];
			const auto sectorInfo = static_cast<SectorInfo*>(sectorPtr);

			new(copySector)SectorInfo(std::move(*sectorInfo));

			for (auto& [typeId, typeIdx] : data.sectorMembersIndexes) {
				const auto memberIdx = data.sectorMembersIndexes.at(typeId);
				if (sectorInfo->isTypeNull(memberIdx)) {
					continue;
				}

				const auto oldPlace = Utils::getTypePlace(sectorPtr, typeId, data.sectorMembersOffsets, data.sectorMembersIndexes);
				const auto newPlace = Utils::getTypePlace(copySector, typeId, data.sectorMembersOffsets, data.sectorMembersIndexes);

				static_cast<ComponentInterface*>(oldPlace)->move(newPlace);//call move constructor
			}
		}
	}

	SectorsChunk::SectorsChunk(const ChunkData& data)
		: data(data)
		, beginAdr(static_cast<char*>(static_cast<void*>(this)) + sizeof(SectorsChunk) + alignof(SectorsChunk))
	{}

	void* SectorsChunk::operator[](size_t i) const {
		return static_cast<char*>(const_cast<void*>(beginAdr)) + i * data.sectorSize;
	}

	void* SectorsChunk::Iterator::operator*() const {
		return mPtr;
	}

	SectorsChunk::Iterator& SectorsChunk::Iterator::operator++() {
		mPtr = static_cast<char*>(mPtr) + mSectorSize;
		return *this;
	}

	SectorsChunk::Iterator& SectorsChunk::Iterator::operator+(size_t i) {
		mPtr = static_cast<char*>(mPtr) + i * mSectorSize;
		return *this;
	}

	bool SectorsChunk::Iterator::operator!=(const Iterator& other) const {
		return mPtr != other.mPtr;
	}

	SectorsChunk::Iterator& SectorsChunk::Iterator::operator=(const Iterator& other) {
		if (this != &other) {
			mPtr = other.mPtr;
			mSectorSize = other.mSectorSize;
		}
		return *this;
	}

	bool SectorsChunk::Iterator::operator==(const Iterator& other) const {
		return mPtr == other.mPtr;
	}

	SectorsChunk::Iterator SectorsChunk::begin() const {
		return Iterator(const_cast<void*>(beginAdr), data);
	}

	SectorsChunk::Iterator SectorsChunk::end() const {
		return Iterator((*this)[size], data);
	}

	void SectorsChunk::shiftDataRight(size_t from) const {
		for (auto i = size - 1; i >= from; i--) {
			auto prevAdr = (*this)[i];
			auto newAdr = (*this)[i + 1];

			const auto sectorInfo = static_cast<SectorInfo*>(prevAdr);

			for (auto& [typeId, typeIdx] : data.sectorMembersIndexes) {
				const auto memberIdx = data.sectorMembersIndexes.at(typeId);
				if (sectorInfo->isTypeNull(memberIdx)) {
					continue;
				}

				const auto oldPlace = Utils::getTypePlace(prevAdr, typeId, data.sectorMembersOffsets, data.sectorMembersIndexes);
				const auto newPlace = Utils::getTypePlace(newAdr, typeId, data.sectorMembersOffsets, data.sectorMembersIndexes);

				static_cast<ComponentInterface*>(oldPlace)->move(newPlace);//call move constructor
			}

			//move data one sector right to empty place for new sector
			new (newAdr)SectorInfo(std::move(*sectorInfo));//call move constructor for sector info

			if (i == 0) {
				break;
			}
		}
	}

	void SectorsChunk::shiftDataLeft(size_t from) const {
		for (auto i = from; i < size - 1; i++) {
			auto newAdr = (*this)[i];
			auto prevAdr = (*this)[i + 1];

			const auto sectorInfo = static_cast<SectorInfo*>(prevAdr);

			for (auto& [typeId, typeIdx] : data.sectorMembersIndexes) {
				const auto memberIdx = data.sectorMembersIndexes.at(typeId);
				if (sectorInfo->isTypeNull(memberIdx)) {
					continue;
				}

				const auto oldPlace = Utils::getTypePlace(prevAdr, typeId, data.sectorMembersOffsets, data.sectorMembersIndexes);
				const auto newPlace = Utils::getTypePlace(newAdr, typeId, data.sectorMembersOffsets, data.sectorMembersIndexes);

				static_cast<ComponentInterface*>(oldPlace)->move(newPlace);//call move constructor
			}

			new (newAdr)SectorInfo(std::move(*sectorInfo));//move sector info
		}
	}
}
