#pragma once

#include "assetsModule/modelModule/Armature.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	struct ArmatureComponent {
		AssetsModule::Armature armature;
		SFE::Math::Mat4 boneMatrices[100];
	};
}

using SFE::ComponentsModule::ArmatureComponent;