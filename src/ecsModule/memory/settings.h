#pragma once

#include "../Types.h"

namespace ECS {
	constexpr size_t ECS_SYSTEM_MEMORY_BUFFER_SIZE = 8ll * 1024u * 1024u;// 8MB
	constexpr size_t ECS_GLOBAL_MEMORY_CAPACITY = 128ll * 1024 * 1024*10; // 128 MB

	constexpr size_t MAX_ENTITIES = 1024*100;

	constexpr EntityId INVALID_ID = std::numeric_limits<EntityId>::max();
}
