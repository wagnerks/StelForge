#include "ComponentsArrayUtils.h"

#include "ComponentsArray.h"


namespace ecss::Memory::Utils {
	void* binarySearch(EntityId sectorId, size_t& idx, ComponentsArray* sectors) {
		auto right = sectors->size();
		if (right == 0) {
			idx = 0;
			return nullptr;
		}
				
		if ((*sectors)[right - 1]->id < sectorId) {
			idx = right;
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

		auto left = 0;

		while (true) {
			const auto dist = right - left;
			if (dist == 1) {
				idx = left + 1;
				break;
			}

			left += dist / 2;

			if ((*sectors)[left]->id > sectorId) {
				right = left;
			}
			else if ((*sectors)[left]->id == sectorId) {
				idx = left;
				return (*sectors)[left];
			}
		}
		
		return nullptr;
	}
}
