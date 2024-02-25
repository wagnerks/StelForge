#pragma once
#include <limits>
#include <string>

namespace AssetsModule {
	class Asset {
	public:
		virtual ~Asset() {}
		Asset() = default;

		size_t assetId = std::numeric_limits<size_t>::max();
		std::string assetPath;
	};
}
