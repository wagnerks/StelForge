#pragma once

#include <cstdint>
#include <type_traits>
#include <limits>

namespace ECS {
	using EntityId = uint32_t;
	using ECSType = uint16_t;

	constexpr EntityId INVALID_ID = std::numeric_limits<EntityId>::max();

	template <class T>
	class StaticTypeCounter {
	private:
		inline static ECSType mCounter = 0;
		inline static size_t mSize = 0;
	public:
		template <class U>
		static ECSType get() {
			static const ECSType STATIC_TYPE_ID{mCounter++};
			return STATIC_TYPE_ID;
		}

		template <class U>
		static size_t getSize() {
			if (std::is_const<U>::value) {
				return 0;
			}
			static const size_t STATIC_TYPE_SIZE{sizeof(U) + alignof(U)};
			mSize += STATIC_TYPE_SIZE;
			return STATIC_TYPE_SIZE;
		}

		static ECSType getCount() {
			return mCounter;
		}

		static size_t getSize() {
			return mSize;
		}
	};

}

