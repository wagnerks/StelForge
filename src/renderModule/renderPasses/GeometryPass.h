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
			TextureHandle outlinesBuffer;
			TextureHandle lightsBuffer;
			TextureHandle normalBuffer;
			TextureHandle albedoBuffer;
			TextureHandle depthsBuffer;

			SamplerHandle sampler;

			FramebufferHandle outlineFramebuffer;
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

		Render::DescriptorSetLayoutHandle gBuffetLayout{};
		Render::DescriptorSetHandle       gBuffetLayoutSet{};

		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		Render::PipelineHandle getGBufferPipeline(const std::string& vertexShader, const std::string& fragmentShader);
		void initGrid();
		void drawGrid(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd);

		bool mInited = false;
		Data mData;
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
