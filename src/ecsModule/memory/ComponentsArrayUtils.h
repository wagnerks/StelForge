#pragma once

#include <array>
#include <unordered_map>

#include "ecsModule/Types.h"

namespace ECS {
	namespace Memory {
		struct SectorsChunk;
	}
}

namespace ECS::Memory::Utils {
	template <class T>
	struct NoHash { std::size_t operator()(T const& s) const { return static_cast<T>(s); } };

	void* getTypePlace(void* start, ECSType typeId, const std::array<uint16_t, 34>& offsets, const std::unordered_map<ECSType, uint8_t, NoHash<ECSType>>& types);
	void* getTypePlace(void* start, uint16_t offset);

	size_t distance(void* beg, void* end, size_t size);

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
