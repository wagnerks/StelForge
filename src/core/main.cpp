#include "Engine.h"
#include "debugModule/imguiDecorator.h"

#if defined(VLD)
#include "vld.h"
#endif

int main() {
	const auto engine = Engine::UnnamedEngine::instance();

	while (engine->isAlive()) {
		engine->update();
	}

	Engine::Debug::ImGuiDecorator::terminate();
	Engine::UnnamedEngine::terminate();

	return 0;
}
