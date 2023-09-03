#include "Engine.h"
#include "debugModule/imguiDecorator.h"

int main() {
	const auto engine = Engine::UnnamedEngine::instance();

	while (engine->isAlive()) {
		engine->update();
	}

	Engine::Debug::ImGuiDecorator::terminate();
	Engine::UnnamedEngine::terminate();

	return 0;
}
