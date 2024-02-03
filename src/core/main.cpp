#include "Engine.h"
#include "debugModule/imguiDecorator.h"

#if defined(VLD)
#include "vld.h"
#endif

int main() {
	const auto engine = SFE::Engine::instance();

	while (engine->isAlive()) {
		engine->update();
	}

	SFE::Debug::ImGuiDecorator::terminate();
	SFE::Engine::terminate();

	return 0;
}
