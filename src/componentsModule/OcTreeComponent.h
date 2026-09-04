#pragma once
#include <vector>
#include "mathModule/Forward.h"

class OcTreeComponent {
public:
	struct ParentTree {
		SFE::Math::Vec3 pos{};
		size_t node = 0;
	};

	bool inOctree = false;
	std::vector<ParentTree> parents;
};

#include "ecss/Types.h"
template<> struct ecss::AllowNonTrivial<OcTreeComponent> : std::true_type {};
