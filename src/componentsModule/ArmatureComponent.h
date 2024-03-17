#pragma once

#include "assetsModule/modelModule/Armature.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	struct ArmatureComponent {
		AssetsModule::Armature armature;
		SFE::Math::Mat4 boneMatrices[100];
	};

	struct ArmatureBonesComponent {
		ArmatureBonesComponent() {
			boneMatrices.resize(100);
		}
		std::vector<SFE::Math::Mat4> boneMatrices;
	};
}

using SFE::ComponentsModule::ArmatureComponent;