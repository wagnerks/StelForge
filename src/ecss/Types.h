#pragma once

#include <cstdint>
#include <limits>

namespace ecss {
	using EntityId = uint32_t;
	using ECSType = uint16_t;

	constexpr EntityId INVALID_ID = std::numeric_limits<EntityId>::max();
}

