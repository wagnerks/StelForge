#pragma once
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "logsModule/logger.h"
#include "renderModule/Framebuffer.h"
#include "renderModule/RenderPass.h"

class Batcher;

namespace SFE::Render::RenderPasses {


	class GeometryPass : public RenderPassWithData {
	public:
		void prepare() override;
		struct Data {
			~Data() {
				/*delete positionBuffer;
				delete viewPositionBuffer;
				delete outlinesBuffer;
				delete lightsBuffer;
				delete normalBuffer;
				delete albedoBuffer;

				delete depthBuffer;*/
			}

			Render::Framebuffer* gFramebuffer; //todo leaks
			AssetsModule::Texture* positionBuffer;
			AssetsModule::Texture* viewPositionBuffer;
			AssetsModule::Texture* outlinesBuffer;
			AssetsModule::Texture* lightsBuffer;
			AssetsModule::Texture* normalBuffer;
			AssetsModule::Texture* albedoBuffer;

			unsigned int rboDepth = 0;
			Render::Framebuffer* outlineFramebuffer;
		};


		
		void init() override;
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:
		bool mInited = false;
		Data mData;
		bool needClearOutlines = false;

		RenderPassDataContainer mOutlineData;
	};
}
