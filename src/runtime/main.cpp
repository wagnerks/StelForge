#include "core/Engine.h"
#include "runtimeBridge/RuntimeBridge.h"
#include "renderModule/GraphicsBackend.h"
#include "windowsModule/Window.h"

#if defined(VLD)
#include "vld.h"
#endif

#include <string_view>

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main(int argc, char** argv)
{
	SFE::Render::configureGraphicsBackend(argc, argv);

	bool serveEditor = false;
	bool visibleWindow = true;
	for (int i = 1; i < argc; ++i) {
		if (!argv[i]) {
			continue;
		}
		const std::string_view arg{ argv[i] };
		if (arg == "--serve") {
			serveEditor = true;
			visibleWindow = false;
		}
		else if (arg == "--headless") {
			visibleWindow = false;
		}
		else if (arg == "--visible") {
			visibleWindow = true;
		}
	}

	if (!glfwInit()) {
		return -1;
	}

	const auto engine = SFE::Engine::instance();
	SFE::Render::WindowHints hints;
	hints.bools[SFE::Render::SCALE_TO_MONITOR] = false;
	if (serveEditor) {
		hints.bools[SFE::Render::DECORATED] = false;
		hints.bools[SFE::Render::FLOATING] = true;
		hints.bools[SFE::Render::RESIZABLE] = false;
		hints.bools[SFE::Render::VISIBLE] = false;
	}
	else if (!visibleWindow) {
		hints.bools[SFE::Render::VISIBLE] = false;
	}
	engine->createWindow(1920, 1080, nullptr, serveEditor ? "StelForgeRuntime" : "StelForge", hints);
	engine->setPresentToScreen(visibleWindow);
	engine->initThread();
	engine->initRender();

	if (serveEditor) {
		SFE::RuntimeBridge::RuntimeBridge::instance()->start(SFE::EditorProtocol::kDefaultPort);
	}

	while (engine->isAlive()) {
		engine->update();
		if (serveEditor) {
			SFE::RuntimeBridge::RuntimeBridge::instance()->tick();
		}
	}

	if (serveEditor) {
		SFE::RuntimeBridge::RuntimeBridge::instance()->stop();
		SFE::RuntimeBridge::RuntimeBridge::terminate();
	}
	SFE::Engine::terminate();
	glfwTerminate();

	return 0;
}
