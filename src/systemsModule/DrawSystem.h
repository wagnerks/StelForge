#pragma once

#include "SystemBase.h"
#include "renderModule/Renderer.h"

namespace Engine::SystemsModule {
	class DrawSystem : public ecss::System<DrawSystem> {
	public:
		void update(float_t dt) override;
	private:
		RenderModule::Renderer mRenderer;
	};
}
