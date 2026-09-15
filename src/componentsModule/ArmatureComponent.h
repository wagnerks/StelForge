#pragma once

#include "assetsModule/modelModule/Armature.h"
#include "ecss/Types.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	struct ArmatureComponent {
		AssetsModule::Armature armature;
	};

	// The pose uploaded while no animation writes the bones on the GPU: one matrix per bone of the armature.
	struct ArmatureBonesComponent {
		std::vector<SFE::Math::Mat4> boneMatrices;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::ArmatureComponent> : std::true_type {};
template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::ArmatureBonesComponent> : std::true_type {};

using SFE::ComponentsModule::ArmatureComponent;