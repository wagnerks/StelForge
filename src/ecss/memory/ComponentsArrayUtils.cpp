#include "ComponentsArrayUtils.h"

#include "ComponentsArray.h"


namespace ecss::Memory::Utils {
	void* binarySearch(EntityId sectorId, size_t& idx, SectorsChunk* sectors) {
		const auto size = sectors->size;
		if (size == 0) {
			idx = 0;
			return nullptr;
		}

		auto begin = sectors->begin();
		auto end = sectors->end();
		
		if (static_cast<SectorInfo*>(static_cast<void*>(static_cast<char*>((*sectors)[size - 1])))->id < sectorId) {
			idx = size;
			return nullptr;
		}

		if (static_cast<SectorInfo*>(static_cast<void*>(static_cast<char*>((*sectors)[0])))->id >= sectorId) {
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
