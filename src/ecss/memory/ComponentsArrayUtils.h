#pragma once

#include <array>
#include <map>

#include "../Types.h"

namespace ecss {
	namespace Memory {
		struct SectorsChunk;
	}
}

namespace ecss::Memory::Utils {
	inline void* getTypePlace(void* start, uint16_t offset) { return static_cast<void*>(static_cast<char*>(start) + offset); }
	inline void* getTypePlace(void* start, ECSType typeId, const std::array<uint16_t, 34>& offsets, const std::map<ECSType, uint8_t>& types) { return getTypePlace(start, offsets[types.at(typeId)]); }

	inline size_t distance(void* beg, void* end, size_t size) { return std::abs((static_cast<char*>(beg) - static_cast<char*>(end))) / size; }

	void* binarySearch(EntityId sectorId, size_t& idx, SectorsChunk* sectors);

	template <typename T>
	struct Base {};

	template <typename... Ts>
	struct TypeSet : Base<Ts>... {
		template<typename T>
		constexpr auto operator+(Base<T>) {
			if constexpr (std::is_base_of_v<Base<T>, TypeSet>) {
				return TypeSet{};
			}
			else {
				return TypeSet<Ts..., T>{};
			}
		}

		static constexpr std::size_t count() {//todo check it works
			return sizeof...(Ts);
		}
	};

	template<typename... Ts>
	constexpr bool areUnique() {
		constexpr auto set = (TypeSet<>{} + ... + Base<Ts>{});
		return set.count() == sizeof...(Ts);
	}
}
