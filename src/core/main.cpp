#include "Engine.h"

#if defined(VLD)
#include "vld.h"
#endif

int main() {
	const auto engine = SFE::Engine::instance();

	while (engine->isAlive()) {
		engine->update();
	}

	SFE::Engine::terminate();

	return 0;
}
