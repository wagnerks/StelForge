#pragma once
#include <limits>

namespace ecsModule {
	static constexpr size_t ECS_SYSTEM_MEMORY_BUFFER_SIZE =	8ll * 1024u * 1024u;// 8MB
	static constexpr size_t ECS_GLOBAL_MEMORY_CAPACITY = 128ll * 1024 * 1024; // 128 MB

	static constexpr size_t COMPONENTS_GROW = 1024;
	static constexpr size_t ENTITIES_GROW = 1024;

	static constexpr size_t INVALID_ID = std::numeric_limits<size_t>::max();
	
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
