#pragma once

#include "rhi/RenderHardwareInterface.h"

namespace SFE {
	namespace SystemsModule {
		struct RenderData;
	}
}

namespace SFE::Render {
	class RenderPass {
	public:
		virtual ~RenderPass() = default;
		virtual void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) = 0;
		virtual void init(SystemsModule::RenderData& renderDataHandle) {}
		// Before the frame's first command list, for every pass that runs this frame: the place to write buffers
		// that live on the device, which Vulkan uploads with the frame's first submit.
		virtual void prepare(SystemsModule::RenderData& renderDataHandle) {}
	};

	struct RenderPassEntry {
		const char* name = "";
		bool enabled = true;
		RenderPass* (*create)() = nullptr;
	};
}
