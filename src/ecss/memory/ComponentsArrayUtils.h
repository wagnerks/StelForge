#pragma once

#include <array>

#include "../Types.h"
#include "../contiguousMap.h"

namespace ecss::Memory {
	class ComponentsArray;
}

namespace ecss::Memory::Utils {
	__forceinline  void* getTypePlace(void* start, uint16_t offset) { return static_cast<void*>(static_cast<char*>(start) + offset + 1); }
	__forceinline  void* getTypePlace(void* start, ECSType typeId, const std::array<uint16_t, 10>& offsets, ContiguousMap<ECSType, uint8_t>& types) { return getTypePlace(start, offsets[types[typeId]]); }

	__forceinline  size_t distance(void* beg, void* end, size_t size) { return std::abs((static_cast<char*>(beg) - static_cast<char*>(end))) / size; }

	void* binarySearch(EntityId sectorId, size_t& idx, ComponentsArray* sectors);
}
