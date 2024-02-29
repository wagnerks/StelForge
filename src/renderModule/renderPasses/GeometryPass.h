#pragma once
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "glWrapper/Framebuffer.h"
#include "glWrapper/Renderbuffer.h"
#include "logsModule/logger.h"
#include "renderModule/RenderPass.h"

class Batcher;

namespace SFE::Render::RenderPasses {


	class GeometryPass : public RenderPassWithData {
	public:
		void prepare() override;
		struct Data {
			GLW::Framebuffer gFramebuffer;
			GLW::Texture positionBuffer{GLW::TEXTURE_2D};
			GLW::Texture viewPositionBuffer{GLW::TEXTURE_2D};
			GLW::Texture outlinesBuffer{GLW::TEXTURE_2D};
			GLW::Texture lightsBuffer{GLW::TEXTURE_2D};
			GLW::Texture normalBuffer{GLW::TEXTURE_2D};
			GLW::Texture albedoBuffer{GLW::TEXTURE_2D};

			GLW::Renderbuffer rboDepth;
			GLW::Framebuffer outlineFramebuffer;
		};


		
		void init() override;
		void render(SystemsModule::RenderData& renderDataHandle) override;
	private:
		bool mInited = false;
		Data mData;
		bool needClearOutlines = false;

		RenderPassDataContainer mOutlineData;
	};
}
