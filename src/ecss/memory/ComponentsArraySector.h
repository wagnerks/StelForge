﻿#pragma once
#include <cstdint>
#include <unordered_map>

#include "ComponentsArrayUtils.h"
#include "../Types.h"

namespace ecss::Memory {
	struct ChunkData {
		uint16_t sectorSize = 0;
		std::array<uint16_t, 34> sectorMembersOffsets;
		std::unordered_map<ECSType, uint8_t, Utils::NoHash<ECSType>> sectorMembersIndexes; // < {type id} , {idx in members offsets} >
	};

	/* SectorsChunk is data container with sectors of custom data in it
	 *
	 * the Sector is object in memory which includes SectorInfo {uint32_t(by default) sectorId; unsigned long nullBits;}
	 * nullBits is the value, which contains information about custom data in sector availability
	 * for example the object on the first place is not created, or deleted, first bit of nullBits == 0, the third object of sector is created, third bit == 1
	 * it allows to return nullptr if bit is zero
	 *
	 * sector stores data for any custom type in theory, offset to type stores in ChunkData struct
	 *--------------------------------------------------------------------------------------------
	 *                                          [SECTOR]
	 * 0x 00                                                           {SectorInfo}
	 * 0x sizeof(SectorInfo)                                           {SomeObject}
	 * 0x sizeof(SectorInfo + SomeObject)                              {SomeObject1}
	 * 0x sizeof(SectorInfo + SomeObject + SomeObject1)                {SomeObject2}
	 * ...
	 * 0x sizeof(SectorInfo... + ...SomeObjectN-1 + SomeObjectN)       {SomeObjectN}
	 *
	 *--------------------------------------------------------------------------------------------
	 *
	 * N can be any number, but here locked on 32 for optimization purposes, if more then 32 - need to adjust nullBits memory
	 *
	 * objects from sector can be pulled as void*, and then casted to desired type, as long as the information about offsets for every object stored according to their type ids
	 * you can't get object without knowing it's type
	 *
	 */
	struct SectorsChunk {
		SectorsChunk(SectorsChunk&& other) noexcept;
		size_t size = 0;
		const ChunkData data;

		const void* beginAdr;

		SectorsChunk(const ChunkData& data);
		void* operator[](size_t i) const;

		//shifts chunk data right
		//[][][][from][][][]   -> [][][] [empty] [from][][][]
		void shiftDataRight(size_t from) const;

		//shifts chunk data left
		//[][][][from][][][]   -> [][][from][][][]
		//caution - shifting on alive data will produce memory leak
		void shiftDataLeft(size_t from) const;

		class Iterator {
		private:
			void* mPtr;
			uint16_t mSectorSize;
		public:
			Iterator() : mPtr(nullptr), mSectorSize(0) {}
			Iterator(void* ptr, const ChunkData& data) : mPtr(ptr), mSectorSize(data.sectorSize) {}

			bool operator!=(const Iterator& other) const;
			bool operator==(const Iterator& other) const;

			Iterator& operator=(const Iterator& other);
			void* operator*() const;

			Iterator& operator++();
			Iterator& operator+(size_t i);
		};

		Iterator begin() const;
		Iterator end() const;
	};

	/*
	*sector stores data for any custom type in theory, offset to type stores in ChunkData struct
	* --------------------------------------------------------------------------------------------
	*                                       [SECTOR]
	* 0x 00                                                         { SectorInfo  }
	* 0x sizeof(SectorInfo)                                         { SomeObject  }
	* 0x sizeof(SectorInfo + SomeObject)                            { SomeObject1 }
	* 0x sizeof(SectorInfo + SomeObject + SomeObject1)              { SomeObject2 }
	* ...
	* 0x sizeof(SectorInfo... + ...SomeObjectN - 1 + SomeObjectN)   { SomeObjectN }
	*
	*--------------------------------------------------------------------------------------------
	*/
	struct SectorInfo {
		EntityId id;
		unsigned long nullBits = 0; //each bit means that component 0 1 2 3 etc alive or not, maximum 32 components

		bool isTypeNull(uint8_t typeIdx) const {
			return !((nullBits >> (typeIdx - 1)) & 1);
		}

		void setTypeBitTrue(uint8_t typeIdx) {
			nullBits |= (1 << (typeIdx - 1)); //typeIdx in sector starts after sectorInfo, so move index 1 to left
		}

		void setTypeBitFalse(uint8_t typeIdx) {
			nullBits &= ~(1 << (typeIdx - 1));
		}
	};

}