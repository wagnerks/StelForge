#pragma once
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "assetsModule/TextureHandler.h"
#include "glWrapper/Framebuffer.h"
#include "glWrapper/Renderbuffer.h"
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

			SamplerHandle sampler;

			FramebufferHandle outlineFramebuffer;
		};

		RenderPassHandle geometryPass;
		PipelineHandle geometryPipeline;

		PipelineHandle gBufferOutlinePipeline;
		RenderPassHandle outlineBufferRenderPass;
		PipelineHandle outlinePipeline;
		RenderPassHandle outlineRenderPass;

		Render::DescriptorSetLayoutHandle mTexSetLayoutOutlines{};
		Render::DescriptorSetHandle       mTexSetOutlines{};


		void init() override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		bool mInited = false;
		Data mData;
		bool needClearOutlines = false;

		RenderPassDataContainer mOutlineData;
	};
}
