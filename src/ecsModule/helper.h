#pragma once
#include <cstdint>
#include <limits>

namespace ecsModule {
	static constexpr size_t ECS_SYSTEM_MEMORY_BUFFER_SIZE =	8ll * 1024u * 1024u;// 8MB
	static constexpr size_t ECS_GLOBAL_MEMORY_CAPACITY = 128ll * 1024 * 1024; // 128 MB

	static constexpr size_t INVALID_ID = std::numeric_limits<size_t>::max();

	static constexpr uint16_t LOWEST_SYSTEM_PRIORITY = std::numeric_limits<uint16_t>::min();

	static constexpr uint16_t VERY_LOW_SYSTEM_PRIORITY = 99;
	static constexpr uint16_t LOW_SYSTEM_PRIORITY = 100;

	static constexpr uint16_t NORMAL_SYSTEM_PRIORITY = 200;

	static constexpr uint16_t MEDIUM_SYSTEM_PRIORITY = 300;

	static constexpr uint16_t HIGH_SYSTEM_PRIORITY = 400;
	static constexpr uint16_t VERY_HIGH_SYSTEM_PRIORITY = 401;

	static constexpr uint16_t HIGHEST_SYSTEM_PRIORITY = std::numeric_limits<uint16_t>::max();

	template <class T>
	class FamilySize {
	private:
		inline static size_t typeCounter = 0;
	public:
		template <class U>
		static size_t Get() {
			static const size_t STATIC_TYPE_ID{typeCounter++};
			return STATIC_TYPE_ID;
		}

		static size_t Get() {
			return typeCounter;
		}
	};
}
