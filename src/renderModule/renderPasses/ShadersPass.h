#pragma once
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class ShadersPass : public RenderPass {
	public:
		ShadersPass();
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;\
		struct DrawData
		{
			Math::Vec4 viewPos;
			float far;
			float near;
		};

		DrawData drawData;
		RenderPassHandle renderPass;
	};
}
