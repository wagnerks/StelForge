#pragma once
#include <mutex>
#include <thread>
#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "componentsModule/MeshComponent.h"
#include "renderModule/FrameBlackboard.h"
#include "renderModule/GpuScene.h"
#include "renderModule/MaterialTextures.h"
#include "renderModule/RenderHardwareInterface.h"

#include "renderModule/renderPasses/RenderPass.h"


namespace SFE::Render::RenderPasses {

	class CascadedShadowPass : public RenderPass {
	public:

		using Data = Render::CsmFrame;

		CascadedShadowPass();
		~CascadedShadowPass() override;
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void initRender();

		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		void updateRenderData(SystemsModule::RenderData& renderDataHandle);
		void debug(SystemsModule::RenderData& renderDataHandle);

		// One framebuffer per cascade layer. The layer used to come from gl_Layer in a geometry
		// shader whose only other job was to copy the triangle through.
		std::vector<FramebufferHandle> lightFbos;
		TextureHandle  lightDepthMa;
		SamplerHandle  lightSampler;
		SamplerHandle  lightDepthSampler;

		BufferHandle matricesUBO;
		// Opaque casters render depth only; anything with a cutout or dithered opacity needs a
		// fragment shader with discard, which costs early-Z, so it gets its own pipeline.
		PipelineHandle pipelineOpaque;
		PipelineHandle pipelineCutout;
		RenderPassHandle renderPass;

		// Set 0 holds the buffers and differs per cascade (each has its own visible list); set 1 holds the
		// caster's mask texture and is shared by every cascade.
		struct Cascade {
			GpuScene::View view;
			DescriptorSetHandle set;
			uint32_t boundIds[4]{};
		};
		Render::DescriptorSetLayoutHandle mBufferLayout;
		Render::DescriptorSetLayoutHandle mTextureLayout;
		std::vector<Cascade> mCascades;
		SingleTextureSets mMaskSets;
		bool mProxyRegistered = false;

		ecss::EntityId mShadowSource;
		bool mInited = false;
		Data mData;

		SFE::MeshObject3D mGlassShadowMesh;
		SFE::RenderMeshData mGlassShadowMeshData;
	};
}

