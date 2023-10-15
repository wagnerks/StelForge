#pragma once
#include <array>
#include <cstdint>

#include "../Types.h"
#include "../contiguousMap.h"

namespace ecss::Memory {
	struct ChunkData {
		uint16_t sectorSize = 0;
		std::array<uint16_t, 10> sectorMembersOffsets = {};
		ContiguousMap<ECSType, uint8_t> sectorMembersIndexes; // < {type id} , {idx in members offsets} >
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

		inline constexpr void setAlive(size_t offset, size_t objSize, bool value) {
			*static_cast<bool*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this)) + offset + objSize - sizeof(bool))) = value;//use last byte which is reserved for align - to store if object alive
		}

		inline constexpr bool isAlive(size_t offset, size_t objSize) {
			return *static_cast<bool*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this)) + offset + objSize - sizeof(bool)));
		}

		template<typename T>
		inline constexpr T* getObject(size_t offset) noexcept {
			return isAlive(offset, sizeof(T) + alignof(T)) ? static_cast<T*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this)) + offset)) : nullptr;
		}
	};

}
