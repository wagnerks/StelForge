#pragma once

#include <cstdint>

namespace ECS {
	using EntityId = uint32_t;
	using ECSType = uint16_t;

	template <class T>
	class StaticTypeCounter {
	private:
		inline static ECSType mCounter = 0;
	public:
		template <class U>
		static ECSType get() {
			static const ECSType STATIC_TYPE_ID{mCounter++};
			return STATIC_TYPE_ID;
		}

		static ECSType getCount() {
			return mCounter;
		}
	};

}

