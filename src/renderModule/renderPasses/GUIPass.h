#pragma once
#include "ecss/Registry.h"
#include "renderModule/renderPasses/RenderPass.h"
#include "renderModule/TextRenderer.h"

namespace SFE::Render::RenderPasses {

	struct PosComponent {
		Math::Vec2 pos;
		Math::Vec2 size;


		Math::Vec2 pivot;
	};

	struct ColorComponent {
		Math::Vec4 color {1.f};
	};

	struct TextComponent {
		std::string text;
		float scale = 1.f;
	};

	struct FontComponent {
		Font* font = nullptr;
	};

	class GUIPass : public RenderPass {
	public:
		~GUIPass() override;
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

		static inline ecss::Registry<> registry;


		PipelineHandle pipeline;

		Render::DescriptorSetLayoutHandle mTexSetLayout{};
		Render::DescriptorSetHandle       mTexSet{};
		Render::RenderPassHandle          mRenderPass{};

		BufferHandle drawDataUBO;
	};
}

