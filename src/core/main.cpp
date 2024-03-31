#include "Engine.h"

#if defined(VLD)
#include "vld.h"
#endif

int main() {
	if (!glfwInit()) {
		return -1;
	}

	const auto engine = SFE::Engine::instance();

	engine->createWindow(1920, 1080);
	engine->initThread();
	engine->initRender();

	while (engine->isAlive()) {
		engine->update();
	}

	SFE::Engine::terminate();
	glfwTerminate();

	return 0;
}
