#include "Engine.h"
#include "debugModule/imguiDecorator.h"

//#include "vld.h"

int main() {
	std::thread renderThread = std::thread([]() {
		const auto engine = Engine::UnnamedEngine::instance();

		while (engine->isAlive()) {
			engine->update();
		}

		Engine::Debug::ImGuiDecorator::terminate();
		Engine::UnnamedEngine::terminate();
	});

	renderThread.join();

	return 0;
}
