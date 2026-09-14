#pragma once

#include <memory>

#include "rhi/GraphicsBackend.h"
#include "rhi/RenderHardwareInterface.h"

namespace SFE::Render {
	// The one place that knows the backend classes. Callers get the interface and never include a
	// backend header.
	std::unique_ptr<RenderHardwareInterface> createRhi(GraphicsBackend backend);
}
