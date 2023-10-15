#pragma once

#include <array>

#include "../Types.h"
#include "../contiguousMap.h"

namespace ecss::Memory {
	class ComponentsArray;
}

namespace ecss::Memory::Utils {
	__forceinline  void* getTypePlace(void* start, uint16_t offset) { return static_cast<void*>(static_cast<char*>(start) + offset); }
	__forceinline  void* getTypePlace(void* start, ECSType typeId, const std::array<uint16_t, 10>& offsets, ContiguousMap<ECSType, uint8_t>& types) { return getTypePlace(start, offsets[types[typeId]]); }

	__forceinline  size_t distance(void* beg, void* end, size_t size) { return std::abs((static_cast<char*>(beg) - static_cast<char*>(end))) / size; }

	void* binarySearch(EntityId sectorId, size_t& idx, ComponentsArray* sectors, size_t sectorSize);

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
