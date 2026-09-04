#pragma once

#include "assetsModule/modelModule/Armature.h"
#include "ecss/Types.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	inline constexpr int kMaxArmatureBones = 256;

	struct ArmatureComponent {
		AssetsModule::Armature armature;
	};

	struct ArmatureBonesComponent {
		ArmatureBonesComponent() {
			boneMatrices.resize(kMaxArmatureBones);
		}

		std::vector<SFE::Math::Mat4> boneMatrices;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::ArmatureComponent> : std::true_type {};
template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::ArmatureBonesComponent> : std::true_type {};

using SFE::ComponentsModule::ArmatureComponent;