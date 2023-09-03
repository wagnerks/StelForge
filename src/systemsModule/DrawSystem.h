#pragma once
#include "ecsModule/SystemBase.h"
#include "renderModule/Renderer.h"

namespace Engine::SystemsModule {
	class DrawSystem : public ecsModule::System<DrawSystem> {
	public:
		void update(float_t dt) override;
	private:
		RenderModule::Renderer mRenderer;
	};
}
