#pragma once
#include <future>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>

#include "assetsModule/TextureHandler.h"
#include "logsModule/logger.h"
#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

class Batcher;

namespace SFE::Render::RenderPasses {


	class GeometryPass : public RenderPassWithData {
	public:
		void prepare() override;
		struct Data {
			FramebufferHandle gFramebuffer;

			TextureHandle positionBuffer;
			TextureHandle viewPositionBuffer;
			TextureHandle normalBuffer;
			TextureHandle albedoBuffer;
			TextureHandle depthsBuffer;
		};

		void publish(SystemsModule::RenderData& renderDataHandle);

		RenderPassHandle geometryPass;
		PipelineHandle geometryPipeline;

		PipelineHandle gBufferOutlinePipeline;
		RenderPassHandle outlineBufferRenderPass;
		PipelineHandle outlinePipeline;
		RenderPassHandle outlineRenderPass;

		Render::DescriptorSetLayoutHandle mTexSetLayoutOutlines{};
		Render::DescriptorSetHandle       mTexSetOutlines{};
		Render::DescriptorSetHandle       mOutlineFrameSet{};

		Render::DescriptorSetLayoutHandle gBuffetLayout{};
		Render::DescriptorSetHandle       gBuffetLayoutSet{};

		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		// The fill mode is part of the key: wireframe is a second pipeline, not a state change. The
		// RHI has no dynamic polygon mode -- Vulkan bakes it into the PSO and cannot override it.
		Render::PipelineHandle getGBufferPipeline(const std::string& vertexShader, const std::string& fragmentShader,
			Render::PolygonFillMode fillMode = Render::PolygonFillMode::Fill);
		void initGrid(Render::DescriptorSetLayoutHandle frameLayout);
		void drawGrid(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd);

		void fillBatcher(Batcher& batcher, uint8_t nextRegistry, SystemsModule::RenderData& renderData);

		bool mInited = false;
		Data mFrames[2];
		uint32_t mGpuFrames = 1;
		SamplerHandle mSampler{};
		TextureHandle outlinesMask{};
		TextureHandle outlinesBuffer{};
		TextureHandle lightsBuffer{};
		FramebufferHandle outlineFramebuffer{};
		FramebufferHandle outlineResultFbo{};
		TextureHandle mBoundOutlineLights{};
		TextureHandle mBoundOutlineNormal{};
		TextureHandle mBoundOutlineMask{};
		bool needClearOutlines = true;

		RenderPassDataContainer mOutlineData;

		GraphicsPipelineDesc mGBufferPipelineDesc;
		std::unordered_map<std::string, PipelineHandle> mGBufferPipelines;

		PipelineHandle mGridPipeline;
		BufferHandle mGridVBO;
		BufferHandle mGridEBO;
		struct GridDrawData {
			Math::Vec4 viewPos;
			float far = 0.f;
			float near = 0.f;
		};
		GridDrawData mGridDrawData;
	};
}
