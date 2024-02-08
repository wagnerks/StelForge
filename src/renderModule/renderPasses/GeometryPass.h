#pragma once
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "renderModule/RenderPass.h"

class Batcher;

namespace SFE::RenderModule::RenderPasses {
	class GeometryPass : public RenderPassWithData {
	public:
		void prepare() override;
		struct Data {
			unsigned int mGBuffer = 0;
			unsigned int gPosition = 0;
			unsigned int gViewPosition = 0;
			unsigned int gOutlines = 0;
			unsigned int gLights = 0;
			unsigned int gNormal = 0;
			unsigned int gAlbedoSpec = 0;
			unsigned int rboDepth = 0;
			unsigned int gDepthTexture = 0;
		};

		struct OutlinesData {
			unsigned int mFramebuffer = 0;
			unsigned int depth = 0;

		};
		void init() override;
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:
		bool mInited = false;
		Data mData;
		OutlinesData mOData;
		bool needClearOutlines = false;

		RenderPassDataContainer mOutlineData;
	};
}
