#pragma once
#include <limits>

namespace AssetsModule {
	class Asset {
	public:
		virtual ~Asset() {}
		Asset() = default;

		size_t assetId = std::numeric_limits<size_t>::max();
	};
}
