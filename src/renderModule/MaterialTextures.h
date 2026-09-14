#pragma once

#include <vector>

#include "componentsModule/MaterialComponent.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render {
	// The material texture bindings (DIFFUSE..SUBSURFACE): the engine's default for every slot first,
	// then the material's own textures over them.
	std::vector<WriteDescriptor> materialTextureWrites(DescriptorSetHandle set, const ComponentsModule::Materials& materials);
}
