#pragma once
#include <mutex>
#include <thread>
#include <vector>

#include "assetsModule/modelModule/Mesh.h"
#include "componentsModule/MeshComponent.h"
#include "renderModule/FrameBlackboard.h"
#include "renderModule/RenderHardwareInterface.h"

#include "renderModule/renderPasses/RenderPass.h"


namespace SFE::Render::RenderPasses {

	class CascadedShadowPass : public RenderPassWithData {
	public:
		void prepare() override;


		using Data = Render::CsmFrame;

		CascadedShadowPass();
		~CascadedShadowPass() override;
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void initRender();

		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		void updateRenderData(SystemsModule::RenderData& renderDataHandle);
		void debug(SystemsModule::RenderData& renderDataHandle);

		FramebufferHandle lightFbo;
		TextureHandle  lightDepthMa;
		SamplerHandle  lightSampler;
		SamplerHandle  lightDepthSampler;

		BufferHandle matricesUBO;
		PipelineHandle pipeline;
		RenderPassHandle renderPass;

		Render::DescriptorSetLayoutHandle shadowsLayoutHandle;
		Render::DescriptorSetHandle       shadowsSetDescHandle;

		ecss::EntityId mShadowSource;
		bool mInited = false;
		Data mData;

		SFE::MeshObject3D mGlassShadowMesh;
		SFE::RenderMeshData mGlassShadowMeshData;
	};
}

