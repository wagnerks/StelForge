#pragma once

#include <cstdio>
#include <string>
#include <thread>

#include "Core.h"
#include "FileSystem.h"
#include "InputHandler.h"
#include "containersModule/Singleton.h"
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
		bool proceduralSpheres() const { return mProceduralSpheres; }

	protected:
		Engine();
		~Engine() override;

	private:
		void fpsSync(int fps) const;
		void updateDelta();
		bool checkNeedClose();

		float mLastFrame = 0.f;
		float mDeltaTime = 0.f;
		float mFramesTimer = 0.f;
		int mFramesCounter = 0;
		int mFPS = 0;


		bool mAlive = false;
		bool mEmbeddedInEditor = false;
		bool mPublishEditorDisplay = false;
		bool mPresentToScreen = false;
		bool mCapturingFrame = false;
		int mCaptureFrame = 0;
		int mLightingDebugTerm = 0;
		bool mTemporalAccumulation = true;
		bool mProceduralSpheres = false;
		std::string mPassDumpDir;

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

