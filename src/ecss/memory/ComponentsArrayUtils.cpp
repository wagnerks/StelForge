#include "ComponentsArrayUtils.h"

#include "ComponentsArray.h"


namespace ecss::Memory::Utils {
	void* binarySearch(EntityId sectorId, size_t& idx, ComponentsArray* sectors, size_t sectorSize) {
		const auto size = sectors->size();
		if (size == 0) {
			idx = 0;
			return nullptr;
		}

		auto begin = sectors->beginSectors();
		auto end = sectors->endSectors();
		
		if ((*sectors)[size - 1]->id < sectorId) {
			idx = size;
			return nullptr;
		}

		if ((*sectors)[0]->id > sectorId) {
			idx = 0;
			return nullptr;
		}

		if ((*sectors)[0]->id == sectorId) {
			idx = 0;
			return (*sectors)[0];
		}

		auto it = begin;

		while (true) {
			it = begin;

			const auto dist = Utils::distance(*begin, *end, sectorSize);
			if (dist == 1) {
				idx = Utils::distance(*sectors->beginSectors(), *it, sectorSize) + 1;
				break;
			}

			it = it + dist / 2;

			if (static_cast<SectorInfo*>(*it)->id > sectorId) {
				end = it;
			}
			else if (static_cast<SectorInfo*>(*it)->id == sectorId) {
				idx = Utils::distance(*sectors->beginSectors(), *it, sectorSize);
				return *it;
			}
			else {
				begin = it;
			}
		}

		return nullptr;
	}
}
