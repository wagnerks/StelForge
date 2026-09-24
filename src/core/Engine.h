#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <cstdio>
#include <string>
#include <thread>

#include "Core.h"
#include "FileSystem.h"
#include "InputHandler.h"
#include "containersModule/Singleton.h"
#include "networkModule/NetworkSettings.h"
#include "rhi/GraphicsBackend.h"
#include "rhi/RenderHardwareInterface.h"

class Camera;

namespace SFE {
	class Engine : public Singleton<Engine>, public CoreModule::InputObserver {
		friend Singleton;

	public:
		Render::Window* createWindow(int width, int height, GLFWwindow* window = nullptr, const std::string& title = "Engine", Render::WindowHints hints = {});
		void setWindow(Render::Window* window);
		void destroyWindow();

		void initThread();
		void initRender();

		void update();

		float getDeltaTime() const;
		// Seconds of animation time: advances by the frame step, and stands still while a frame is captured.
		double animationClock() const { return mAnimationClock; }
		int getFPS() const;

		bool isAlive() const;

		GLFWwindow* getMainWindow() const;
		Render::Window* getWindow() const;
		static bool isMainThread();
		static double clockSeconds();


		static void reproTrace(const char* msg) {
			const auto path = FileSystem::resolve("hang.trace").string();
			if (FILE* f = nullptr; fopen_s(&f, path.c_str(), "a") == 0 && f) {
				std::fprintf(f, "%s\n", msg);
				std::fflush(f);
				std::fclose(f);
			}
		}

		int maxFPS = 600;

		Render::RenderHardwareInterface* rhi() const { return mRHI; }

		void setEmbeddedInEditor(bool embedded) { mEmbeddedInEditor = embedded; }
		bool isEmbeddedInEditor() const { return mEmbeddedInEditor; }
		void setPublishEditorDisplay(bool publish) { mPublishEditorDisplay = publish; }
		bool publishEditorDisplay() const { return mPublishEditorDisplay; }
		void setPresentToScreen(bool present) { mPresentToScreen = present; }
		bool presentToScreen() const { return mPresentToScreen; }
		// Escape closes the window, unless the app handles it itself.
		void setEscapeCloses(bool closes) { mEscapeCloses = closes; }
		// Set for a deterministic frame capture: passes must drop anything that differs from one
		// run to the next (FPS counters, timing overlays) so two captures compare pixel for pixel.
		void setCapturingFrame(bool capturing) { mCapturingFrame = capturing; }
		bool capturingFrame() const { return mCapturingFrame; }
		// Frame index the capture lands on, and where per-pass dumps go (empty = no per-pass
		// dump). RenderSystem needs both to snapshot each pass on exactly that frame.
		void setCaptureFrame(int frame) { mCaptureFrame = frame; }
		int captureFrame() const { return mCaptureFrame; }
		void setPassDumpDir(const std::string& dir) { mPassDumpDir = dir; }
		const std::string& passDumpDir() const { return mPassDumpDir; }
		// Non-zero makes the lighting shader output one isolated term (see deferred_shading.fs)
		// instead of the lit result, so backend parity can be chased term by term.
		void setLightingDebugTerm(int term) { mLightingDebugTerm = term; }
		int lightingDebugTerm() const { return mLightingDebugTerm; }
		// TAA accumulates across frames, so it turns any sub-LSB difference into a moving target.
		// Turning it off measures what a single frame actually produces.
		void setTemporalAccumulation(bool enabled) { mTemporalAccumulation = enabled; }
		bool temporalAccumulation() const { return mTemporalAccumulation; }
		// World chunks spawn the glass sphere grid instead of the character grid: the scene that has glass.
		void setProceduralSpheres(bool enabled) { mProceduralSpheres = enabled; }
		void setProceduralRooms(bool enabled) { mProceduralRooms = enabled; }
		// Non-zero: every frame advances exactly this many seconds, as fast as it can, and tick systems step
		// from the frame instead of their own wall-clock threads -- frame N is the same world on every run.
		// Set before initThread.
		void setFixedDeltaTime(float seconds) { mFixedDeltaTime = seconds; }
		float fixedDeltaTime() const { return mFixedDeltaTime; }
		bool proceduralSpheres() const { return mProceduralSpheres; }
		// A scene file, relative to the content root, loaded instead of the procedural chunks.
		void setScenePath(std::string path) { mScenePath = std::move(path); }
		const std::string& scenePath() const { return mScenePath; }
		// Online play, read by NetworkSystem as it starts: set before initThread.
		void setNetwork(Network::Settings settings) { mNetwork = std::move(settings); }
		const Network::Settings& network() const { return mNetwork; }
		bool proceduralRooms() const { return mProceduralRooms; }

	protected:
		Engine();
		~Engine() override;

	private:
		void fpsSync(int fps) const;
		void updateDelta();
		bool checkNeedClose();
		void addRuntimeCommands();

		// Seconds, as a double: a float clock an hour in steps a quarter of a millisecond, and the frame times with it.
		double mLastFrame = 0.0;
		float mDeltaTime = 0.f;
		// The last few frames' real durations, which the step is the mean of; see updateDelta.
		std::array<float, 8> mRecentDeltas{};
		size_t mRecentCount = 0;
		size_t mRecentNext = 0;
		double mAnimationClock = 0.0;
		float mFramesTimer = 0.f;
		int mFramesCounter = 0;
		int mFPS = 0;


		bool mAlive = false;
		bool mEmbeddedInEditor = false;
		bool mPublishEditorDisplay = false;
		bool mPresentToScreen = false;
		bool mEscapeCloses = true;
		bool mCapturingFrame = false;
		int mCaptureFrame = 0;
		int mLightingDebugTerm = 0;
		bool mTemporalAccumulation = true;
		bool mProceduralSpheres = false;
		bool mProceduralRooms = false;
		float mFixedDeltaTime = 0.f;
		std::string mPassDumpDir;
		std::string mScenePath;
		Network::Settings mNetwork;

		CoreModule::Core mCore;
		
		Render::Window* mWindow = nullptr;
		inline static std::thread::id mMainThreadID;

		// The RHI asks its host for the window and the present flags instead of reaching for this
		// singleton. This is the engine answering; a test answers with its own stub.
		class RhiHostAdapter final : public Render::RhiHost {
		public:
			explicit RhiHostAdapter(Engine* engine) : mEngine(engine) {}
			Render::RhiSurface surface() const override;
			bool presentToScreen() const override { return mEngine->presentToScreen(); }
			std::string loadShaderSource(std::string_view path) const override;
			std::filesystem::path shaderCacheDir() const override;
			void onShaderCompile(std::string_view name, bool finished) const override;
		private:
			Engine* mEngine = nullptr;
		};
		RhiHostAdapter mRhiHost{ this };

		Render::RenderHardwareInterface* mRHI = nullptr;
	};
}

