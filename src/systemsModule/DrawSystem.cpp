#include "DrawSystem.h"

namespace Engine::SystemsModule {
	void DrawSystem::update(float_t dt) {
		mRenderer.draw();
	}
}

