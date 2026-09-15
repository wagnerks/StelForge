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
	};

	struct RenderPassEntry {
		const char* name = "";
		bool enabled = true;
		RenderPass* (*create)() = nullptr;
	};
}
