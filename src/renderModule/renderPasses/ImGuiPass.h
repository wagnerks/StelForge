#pragma once

#include "renderModule/ImGuiRHI.h"
#include "renderModule/renderPasses/RenderPass.h"

struct ImGuiContext;

namespace SFE::Render::RenderPasses {
	class ImGuiPass : public RenderPass {
	public:
		~ImGuiPass() override;
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		void ensureContext();

		ImGuiRHI mRenderer;
		ImGuiContext* mContext = nullptr;
		ImGuiContext* mPrevious = nullptr;
		bool mGlfw = false;
	};
}
