#include "rhi/RhiFactory.h"

#include "rhi/OpenGLRHI.h"
#include "rhi/VulkanRHI.h"

namespace SFE::Render {
	std::unique_ptr<RenderHardwareInterface> createRhi(const GraphicsBackend backend) {
		switch (backend) {
		case GraphicsBackend::Vulkan:
			return std::make_unique<VulkanRHI>();
		default:
			return std::make_unique<OpenGLRHI>();
		}
	}
}
