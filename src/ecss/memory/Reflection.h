#pragma once
#include <functional>

#include "../Types.h"
#include "../contiguousMap.h"

namespace ecss::Memory {
	class ReflectionHelper {
	public:
		inline static ContiguousMap<ECSType, std::function<void(void* dest, void* src)>> moveMap;
		inline static ContiguousMap<ECSType, std::function<void(void* src)>> destructorMap;

		template<typename T>
		static ECSType getTypeId() {
			return getTypeIdIml<std::remove_const_t<std::remove_pointer_t<T>>>();
		}

	private:
		inline static ECSType mTypes = 0;

		template<typename T>
		static ECSType initType() {
			const ECSType id = mTypes++;

			moveMap[id] = [](void* dest, void* src) { new(dest)T(std::move(*static_cast<T*>(src))); };
			destructorMap[id] = [](void* src) { static_cast<T*>(src)->~T(); };

			return id;
		}

		template<typename T>
		static ECSType getTypeIdIml() {
			static ECSType id = initType<T>();
			return id;
		}
	};

}
