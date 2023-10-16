#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "../Types.h"
#include "../contiguousMap.h"

namespace ecss::Memory {
	struct ChunkData {
		uint16_t sectorSize = 0;
		std::vector<uint16_t> sectorMembersOffsets = {};
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

		inline constexpr void setAlive(size_t offset, bool value) {
			*static_cast<bool*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this)) + offset)) = value;//use first byte which is also reserved for align - to store if object alive
		}

		inline constexpr bool isAlive(size_t offset) {
			return *static_cast<bool*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this)) + offset));
		}

		template<typename T>
		inline constexpr T* getObject(size_t offset) noexcept {
			const auto alive = static_cast<bool*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this)) + offset));
			return *alive ? static_cast<T*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(alive)) + 1)) : nullptr;
		}
	};

}
