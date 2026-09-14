#include "rhi/RhiFactory.h"

#include "rhi/NullRHI.h"
#include "rhi/OpenGLRHI.h"
#include "rhi/VulkanRHI.h"

namespace SFE::Render {
	std::unique_ptr<RenderHardwareInterface> createRhi(const GraphicsBackend backend) {
		switch (backend) {
		case GraphicsBackend::OpenGL:
			return std::make_unique<OpenGLRHI>();
		case GraphicsBackend::Vulkan:
			return std::make_unique<VulkanRHI>();
		case GraphicsBackend::None:
			return std::make_unique<NullRHI>();
		}
		return nullptr;
	}
}
