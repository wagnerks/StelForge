#include "core/FileSystem.h"
#include "core/NativeLibraries.h"
#include "core/Engine.h"
#include "runtimeBridge/RuntimeBridge.h"
#include "renderModule/FrameCapture.h"
#include "rhi/GraphicsBackend.h"
#include "windowsModule/Window.h"
#include "core/ECSHandler.h"
#include "systemsModule/systems/CameraSystem.h"
#include "componentsModule/TransformComponent.h"

#if defined(VLD)
#include "vld.h"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <io.h>
#endif

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {
	FILE* gCrashLog = nullptr;
	FILE* gReproLog = nullptr;

	void writeLine(FILE* f, const char* msg) {
		if (!f || !msg) {
			return;
		}
		std::fprintf(f, "%s\n", msg);
		std::fflush(f);
	}

#if defined(_WIN32)
	void writeCrash(FILE* f, EXCEPTION_POINTERS* info) {
		if (!f || !info || !info->ExceptionRecord) {
			return;
		}
		const auto addr = reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress);
		const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
		std::fprintf(f, "CRASH code=0x%08lX addr=%p rva=0x%llX\n",
			static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode),
			info->ExceptionRecord->ExceptionAddress,
			static_cast<unsigned long long>(addr >= base ? addr - base : 0));
		std::fflush(f);
	}

	LONG WINAPI crashFilter(EXCEPTION_POINTERS* info) {
		writeCrash(gCrashLog, info);
		writeCrash(gReproLog, info);
		return EXCEPTION_EXECUTE_HANDLER;
	}
#endif

	void crashLog(const char* msg) {
		writeLine(gCrashLog, msg);
		writeLine(gReproLog, msg);
	}
}

int main(int argc, char** argv)
{
	SFE::NativeLibraries::init();
	if (!SFE::Render::configureGraphicsBackend(argc, argv)) {
		return 2;
	}

	bool serveEditor = false;
	bool visibleWindow = true;
	bool explicitlyVisible = false;
	bool reproHang = false;
	std::string capturePath;
	std::string passDumpDir;
	int captureFrame = 240;
	int lightingDebugTerm = 0;
	bool temporalAccumulation = true;
	bool proceduralSpheres = false;
	bool proceduralRooms = false;
	// Timing run: average the frame and per-pass times over the last two thirds of this many frames, write
	// them to perfOut and exit. An ordinary run, not a capture, so nothing waits for the GPU between frames.
	int perfFrames = 0;
	// Leave after this many frames with exit code 0; 0 runs until closed.
	int frameLimit = 0;
	float fixedDeltaTime = 0.f;
	std::string perfOut = "perf.txt";
	std::string cameraPos;
	std::string cameraRot;
	std::string contentDir;
	std::string scenePath;
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
			explicitlyVisible = true;
		}
		else if (arg == "--capture" && i + 1 < argc && argv[i + 1]) {
			capturePath = argv[++i];
		}
		else if (arg.starts_with("--capture=")) {
			capturePath = std::string(arg.substr(std::string_view("--capture=").size()));
		}
		else if (arg == "--capture-frame" && i + 1 < argc && argv[i + 1]) {
			captureFrame = std::atoi(argv[++i]);
		}
		else if (arg.starts_with("--capture-frame=")) {
			captureFrame = std::atoi(std::string(arg.substr(std::string_view("--capture-frame=").size())).c_str());
		}
		else if (arg == "--camera" && i + 1 < argc && argv[i + 1]) {
			cameraPos = argv[++i];
		}
		else if (arg == "--camera-rot" && i + 1 < argc && argv[i + 1]) {
			cameraRot = argv[++i];
		}
		else if (arg == "--no-taa") {
			temporalAccumulation = false;
		}
		else if (arg == "--rooms") {
			proceduralRooms = true;
		}
		else if (arg == "--scene" && i + 1 < argc && argv[i + 1]) {
			scenePath = argv[++i];
		}
		else if (arg == "--spheres") {
			proceduralSpheres = true;
		}
		else if (arg == "--perf-frames" && i + 1 < argc && argv[i + 1]) {
			perfFrames = std::atoi(argv[++i]);
		}
		else if (arg == "--perf-out" && i + 1 < argc && argv[i + 1]) {
			perfOut = argv[++i];
		}
		else if (arg == "--debug-term" && i + 1 < argc && argv[i + 1]) {
			lightingDebugTerm = std::atoi(argv[++i]);
		}
		else if (arg.starts_with("--debug-term=")) {
			lightingDebugTerm = std::atoi(std::string(arg.substr(std::string_view("--debug-term=").size())).c_str());
		}
		else if (arg == "--capture-passes" && i + 1 < argc && argv[i + 1]) {
			passDumpDir = argv[++i];
		}
		else if (arg.starts_with("--capture-passes=")) {
			passDumpDir = std::string(arg.substr(std::string_view("--capture-passes=").size()));
		}
		else if (arg == "--frames" && i + 1 < argc && argv[i + 1]) {
			frameLimit = std::atoi(argv[++i]);
		}
		else if (arg == "--fixed-dt" && i + 1 < argc && argv[i + 1]) {
			fixedDeltaTime = static_cast<float>(std::atof(argv[++i]));
		}
		else if (arg == "--repro-hang") {
			reproHang = true;
		}
		else if (arg == "--content-dir" && i + 1 < argc && argv[i + 1]) {
			contentDir = argv[++i];
		}
		else if (arg.starts_with("--content-dir=")) {
			contentDir = std::string(arg.substr(std::string_view("--content-dir=").size()));
		}
	}

	// A capture is meant to be reproducible from a script, so it defaults to a hidden window:
	// nothing can steal focus or occlude it, and the pixels come from the render target anyway.
	if (!capturePath.empty()) {
		if (captureFrame < 1) {
			captureFrame = 1;
		}
		if (!explicitlyVisible) {
			visibleWindow = false;
		}
	}

	if (contentDir.empty()) {
		if (const char* env = std::getenv("STELFORGE_CONTENT")) {
			contentDir = env;
		}
		else {
			contentDir = SFE::FileSystem::guessContentRoot().string();
		}
	}
	SFE::FileSystem::setRoot(contentDir);

#if defined(_WIN32)
	CopyFileA(SFE::FileSystem::resolve("crash.log").string().c_str(),
		SFE::FileSystem::resolve("crash.prev.log").string().c_str(), FALSE);
#endif
	if (fopen_s(&gCrashLog, SFE::FileSystem::resolve("crash.log").string().c_str(), "w") != 0) {
		gCrashLog = nullptr;
	}
#if defined(_WIN32)
	SetUnhandledExceptionFilter(crashFilter);
#endif
	if (reproHang) {
		if (fopen_s(&gReproLog, SFE::FileSystem::resolve("repro.log").string().c_str(), "w") != 0) {
			gReproLog = nullptr;
		}
#if defined(_MSC_VER)
		if (gReproLog) {
			const auto handle = reinterpret_cast<_HFILE>(_get_osfhandle(_fileno(gReproLog)));
			_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
			_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
			_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(_CRT_WARN, handle);
			_CrtSetReportFile(_CRT_ERROR, handle);
			_CrtSetReportFile(_CRT_ASSERT, handle);
			_set_abort_behavior(0, _WRITE_ABORT_MSG);
		}
#endif
	}
	crashLog(reproHang ? "repro start" : "start");
#if defined(_WIN32)
	crashLog(GetCommandLineA());
#endif
	crashLog(SFE::FileSystem::root().string().c_str());

	// The None backend has no window, so it needs no GLFW -- and runs where there is no display.
	const bool useGlfw = SFE::Render::activeGraphicsBackend() != SFE::Render::GraphicsBackend::None;
	if (useGlfw && !glfwInit()) {
		crashLog("glfwInit failed");
		if (gCrashLog) {
			std::fclose(gCrashLog);
			gCrashLog = nullptr;
		}
		if (gReproLog) {
			std::fclose(gReproLog);
			gReproLog = nullptr;
		}
		return -1;
	}
	crashLog("glfwInit ok");

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
	std::string title = serveEditor ? "StelForgeRuntime" : "StelForge";
	title += " [";
	title += SFE::Render::graphicsBackendName(SFE::Render::activeGraphicsBackend());
	title += "]";
	engine->createWindow(1920, 1080, nullptr, title, hints);
	if (!engine->getWindow() || (useGlfw && !engine->getMainWindow())) {
		crashLog("createWindow failed");
		if (useGlfw) {
			glfwTerminate();
		}
		if (gCrashLog) {
			std::fclose(gCrashLog);
			gCrashLog = nullptr;
		}
		return -1;
	}
	crashLog("createWindow ok");
	engine->setPresentToScreen(visibleWindow);
	engine->setCapturingFrame(!capturePath.empty());
	SFE::ThreadPool::instance()->setRenderTasksInline(!capturePath.empty());
	engine->setCaptureFrame(captureFrame);
	engine->setLightingDebugTerm(lightingDebugTerm);
	engine->setTemporalAccumulation(temporalAccumulation);
	engine->setProceduralSpheres(proceduralSpheres);
	engine->setProceduralRooms(proceduralRooms);
	engine->setScenePath(scenePath);
	engine->setFixedDeltaTime(fixedDeltaTime);
	if (!capturePath.empty() && !passDumpDir.empty()) {
		const std::filesystem::path requested(passDumpDir);
		engine->setPassDumpDir(requested.is_absolute()
			? requested.string()
			: SFE::FileSystem::resolve(passDumpDir).string());
	}
	crashLog("initThread");
	engine->initThread();
	crashLog("initThread ok");

	// Down a street, where the walls stand between the camera and most of the scene: the point of the scene is that
	// three quarters of it is behind something. --camera still overrides this.
	if (proceduralRooms && cameraPos.empty() && cameraRot.empty()) {
		cameraPos = "0,14,150";
		cameraRot = "-2,0,0";
	}

	// Place the camera from the command line so a capture can frame whatever is being
	// investigated without editing and rebuilding CameraSystem's defaults.
	if (!cameraPos.empty() || !cameraRot.empty()) {
		auto parseVec3 = [](const std::string& text, SFE::Math::Vec3& out) {
			return std::sscanf(text.c_str(), "%f,%f,%f", &out.x, &out.y, &out.z) == 3;
		};
		if (auto* camSys = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()) {
			if (auto transform = ECSHandler::registry().pinComponent<SFE::ComponentsModule::TransformComponent>(camSys->getCurrentCamera())) {
				SFE::Math::Vec3 value{};
				if (!cameraPos.empty() && parseVec3(cameraPos, value)) {
					transform->setPos(value);
				}
				if (!cameraRot.empty() && parseVec3(cameraRot, value)) {
					transform->setRotate(value);
				}
				transform->reloadTransform();
			}
		}
	}

	engine->initRender();
	crashLog("initRender ok");
	if (!visibleWindow) {
		engine->getWindow()->setShouldClose(false);
	}

	if (serveEditor) {
		SFE::RuntimeBridge::RuntimeBridge::instance()->start(SFE::EditorProtocol::kDefaultPort);
	}

	if (reproHang) {
		SFE::SystemsModule::CameraSystem::sReproFly.store(true, std::memory_order_relaxed);
	}

	std::atomic<long long> heartbeat{ 0 };
	std::atomic<bool> watchdogRun{ true };
	std::thread watchdog;
	if (reproHang) {
		watchdog = std::thread([&] {
			long long last = -1;
			int stalled = 0;
			while (watchdogRun.load(std::memory_order_relaxed)) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				const auto now = heartbeat.load(std::memory_order_relaxed);
				if (now == last) {
					++stalled;
					if (stalled >= 8) {
						if (gReproLog) {
							std::fprintf(gReproLog, "HUNG heartbeat=%lld stalled=%ds\n", now, stalled);
							std::fflush(gReproLog);
						}
						std::abort();
					}
				}
				else {
					stalled = 0;
					last = now;
				}
			}
		});
	}

	{
		char cfg[256];
		std::snprintf(cfg, sizeof(cfg), "loop cfg backend=%s present=%d visible=%d shouldClose=%d alive=%d",
			SFE::Render::graphicsBackendName(SFE::Render::activeGraphicsBackend()),
			engine->presentToScreen() ? 1 : 0,
			engine->getWindow() && engine->getWindow()->isVisible() ? 1 : 0,
			engine->getWindow()->isClosing() ? 1 : 0,
			engine->isAlive() ? 1 : 0);
		crashLog(cfg);
	}
	crashLog("entering loop");
	const auto reproStart = std::chrono::steady_clock::now();
	int frames = 0;
	struct PerfTotals {
		double frameMs = 0, cpuMs = 0, prepareMs = 0, waitMs = 0, submitMs = 0, presentMs = 0, gpuMs = 0;
		struct Pass {
			std::string name;
			double cpuMs = 0, gpuMs = 0;
		};
		std::vector<Pass> passes;
		int samples = 0;
	} perf;
	auto perfLast = std::chrono::steady_clock::now();
	while (engine->isAlive()) {
		if (!capturePath.empty()) {
			SFE::ThreadPool::instance()->waitIdle();
			// No frame overlaps the previous one on the GPU: with frames in flight, per-frame data written
			// for this frame could land while the last one is still reading it.
			if (auto* rhi = engine->rhi()) {
				rhi->waitIdle();
			}
		}
		engine->update();
		heartbeat.store(++frames, std::memory_order_relaxed);

		if (perfFrames > 0) {
			const auto now = std::chrono::steady_clock::now();
			const double frameMs = std::chrono::duration<double, std::milli>(now - perfLast).count();
			perfLast = now;
			if (frames > perfFrames / 3) {
				const auto* info = SFE::SystemsModule::RenderDebugInfo::instance();
				perf.frameMs += frameMs;
				perf.cpuMs += info->cpuMs;
				perf.prepareMs += info->prepareMs;
				perf.waitMs += info->waitMs;
				perf.submitMs += info->submitMs;
				perf.presentMs += info->presentMs;
				perf.gpuMs += info->gpuMs;
				for (int p = 0; p < info->passCount; ++p) {
					const std::string name = info->passTimes[p].name;
					auto it = std::find_if(perf.passes.begin(), perf.passes.end(), [&](const auto& e) { return e.name == name; });
					if (it == perf.passes.end()) {
						perf.passes.push_back({ name });
						it = perf.passes.end() - 1;
					}
					it->cpuMs += info->passTimes[p].cpuMs;
					it->gpuMs += info->passTimes[p].gpuMs;
				}
				++perf.samples;
			}
			if (frames >= perfFrames) {
				if (FILE* f = std::fopen(SFE::FileSystem::resolve(perfOut).string().c_str(), "w")) {
					const double n = (std::max)(1, perf.samples);
					std::fprintf(f, "frames %d\nframe %.2f ms (%.1f fps)\nrender cpu %.2f\nsubmit %.2f\npresent %.2f\ngpu %.2f\n",
						perf.samples, perf.frameMs / n, 1000.0 * n / (std::max)(1e-3, perf.frameMs), perf.cpuMs / n, perf.submitMs / n, perf.presentMs / n, perf.gpuMs / n);
					std::fprintf(f, "prepare %.2f\nwait %.2f\n", perf.prepareMs / n, perf.waitMs / n);
					std::fprintf(f, "  %-16s %6s %6s\n", "pass", "cpu", "gpu");
					for (const auto& pass : perf.passes) {
						std::fprintf(f, "  %-16s %6.2f %6.2f\n", pass.name.c_str(), pass.cpuMs / n, pass.gpuMs / n);
					}
					std::fclose(f);
				}
				break;
			}
		}

		if (frameLimit > 0 && frames >= frameLimit) {
			break;
		}
		if (!capturePath.empty() && frames >= captureFrame) {
			const bool ok = SFE::Render::captureFrameToPng(capturePath);
			crashLog(ok ? "capture ok" : "capture failed");
			// Headless keeps isAlive() true regardless of the close flag, so leave explicitly.
			break;
		}
		if (serveEditor) {
			SFE::RuntimeBridge::RuntimeBridge::instance()->tick();
		}
		if (reproHang) {
			if ((frames % 30) == 0) {
				SFE::Math::Vec3 pos{};
				if (auto* camSys = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()) {
					if (auto tc = ECSHandler::registry().pinComponent<const SFE::ComponentsModule::TransformComponent>(camSys->getCurrentCamera())) {
						pos = tc->getPos(true);
					}
				}
				if (gReproLog) {
					const auto sec = std::chrono::duration<float>(std::chrono::steady_clock::now() - reproStart).count();
					std::fprintf(gReproLog, "t=%.2f f=%d pos=%.1f %.1f %.1f chunk=%d %d\n",
						sec, frames, pos.x, pos.y, pos.z,
						static_cast<int>(std::floor(pos.x / 256.f)),
						static_cast<int>(std::floor(pos.z / 256.f)));
					std::fflush(gReproLog);
				}
			}
			if (std::chrono::steady_clock::now() - reproStart > std::chrono::seconds(25)) {
				if (gReproLog) {
					std::fprintf(gReproLog, "repro finished ok frames=%d\n", frames);
					std::fflush(gReproLog);
				}
				engine->getWindow()->close();
			}
		}
	}

	{
		char exitLine[160];
		std::snprintf(exitLine, sizeof(exitLine), "loop exit frames=%d shouldClose=%d",
			frames,
			engine->getWindow() && engine->getWindow()->isClosing() ? 1 : 0);
		crashLog(exitLine);
	}
	watchdogRun.store(false, std::memory_order_relaxed);
	if (watchdog.joinable()) {
		watchdog.join();
	}

	if (serveEditor) {
		SFE::RuntimeBridge::RuntimeBridge::instance()->stop();
		SFE::RuntimeBridge::RuntimeBridge::terminate();
	}
	crashLog("before terminate");
	SFE::Engine::terminate();
	crashLog("after terminate");
	if (useGlfw) {
		glfwTerminate();
	}
	if (gCrashLog) {
		std::fclose(gCrashLog);
		gCrashLog = nullptr;
	}
	if (gReproLog) {
		std::fclose(gReproLog);
		gReproLog = nullptr;
	}

	return 0;
}
