#include "ComponentsArrayUtils.h"

#include "ComponentsArray.h"


namespace ecss::Memory::Utils {
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
		auto size = sectors->size;
		if (begin == end) {
			idx = 0;
			return nullptr;
		}

		if (static_cast<SectorInfo*>(static_cast<void*>(static_cast<char*>((*sectors)[size - 1])))->id < sectorId) {
			idx = size;
			return nullptr;
		}

		if (static_cast<SectorInfo*>(static_cast<void*>(static_cast<char*>((*sectors)[0])))->id > sectorId) {
			idx = 0;
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
