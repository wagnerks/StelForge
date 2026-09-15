#pragma once
#include <vector>
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render {
	struct Font;
}

namespace SFE::Render::RenderPasses {
	class FinalPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		PipelineHandle makeFullscreen(RenderHardwareInterface* rhi, RenderPassHandle renderPass, const char* vsPath, const char* fsPath);

		// The debug overlay: the text, and behind it a graph of the last few seconds of frame and GPU times.
		// Pixels, with the origin at the top left of the render target.
		struct Rect {
			float x = 0.f;
			float y = 0.f;
			float width = 0.f;
			float height = 0.f;
		};
		void drawOverlay(Render::CommandListHandle cmd, int screenWidth, int screenHeight);
		void addRect(const Rect& rect, std::vector<Math::Vec2>& out) const;
		// `samples` is a ring starting at `cursor`, drawn left to right.
		void addGraph(const Rect& rect, const float* samples, int count, int cursor, float scale, std::vector<Math::Vec2>& out) const;
		void drawShapes(Render::CommandListHandle cmd, PipelineHandle pipeline, const std::vector<Math::Vec2>& vertices, const Math::Vec4& color);

		PipelineHandle mOverlayFillPipeline{}; // triangles: the panel behind the graphs
		PipelineHandle mOverlayLinePipeline{}; // line segments: the graphs themselves
		std::vector<Math::Vec2> mOverlayVertices; // kept between frames so building the graph does not allocate
		float mScreenWidth = 0.f;
		float mScreenHeight = 0.f;

		PipelineHandle mBloomPipeline{};
		PipelineHandle mPostPipeline{};
		RenderPassHandle mBloomPass{};
		RenderPassHandle mDisplayPass{};
		DescriptorSetLayoutHandle mLayout{};
		std::vector<DescriptorSetHandle> mBloomSets; // one per frame slot
		std::vector<DescriptorSetHandle> mPostSets; // one per frame slot
		SamplerHandle mSampler{};

		struct PostParams {
			float bloomStrength = 0.62f;
			float exposure = 0.92f;
			float vignette = 0.14f;
			float ca = 0.0028f;
		} mPostParams;

		TextureHandle mBloomTex{};
		FramebufferHandle mBloomFbo{};
		TextureHandle mDisplayTex{};
		FramebufferHandle mDisplayFbo{};
		Font* mOverlayFont = nullptr;
	};
}
