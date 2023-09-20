#pragma once
#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class ShadersPass : public RenderPass {
	public:
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	};
}
