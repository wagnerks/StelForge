#pragma once
#include "renderModule/RenderPass.h"

namespace GameEngine::RenderModule::RenderPasses {
	class GeometryPass : public RenderPass {
	public:
		struct Data {
			unsigned int mGBuffer = 0;
			unsigned int gPosition = 0;
			unsigned int gViewPosition = 0;
			unsigned int gNormal = 0;
			unsigned int gAlbedoSpec = 0;
			unsigned int rboDepth = 0;
		};

		void init();
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	private:
		bool mInited = false;
		Data mData;
	};
}
