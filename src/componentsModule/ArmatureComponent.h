#pragma once

#include "assetsModule/modelModule/Armature.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	struct ArmatureComponent {
		AssetsModule::Armature armature;
	};

	struct ArmatureBonesComponent {
		ArmatureBonesComponent() {
			boneMatrices.resize(100, {1.f});
		}

		std::vector<SFE::Math::Mat4> boneMatrices;
	};
}

using SFE::ComponentsModule::ArmatureComponent;