#pragma once
#include <functional>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "windowsModule/ViewportData.h"
#include "containersModule/Singleton.h"

constexpr int GLFW_CONTEXT_VER_MAJ = 4;
#ifdef __APPLE__
constexpr int GLFW_CONTEXT_VER_MIN = 1; //apple supports maximum 4.1 opengl...
#else
constexpr int GLFW_CONTEXT_VER_MIN = 6;
#endif

namespace SFE::Render {

	enum glfwHintsBool {											//   default					    possible values
		RESIZABLE = GLFW_RESIZABLE,									//  GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		VISIBLE = GLFW_VISIBLE,										//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		DECORATED = GLFW_DECORATED,									//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		FOCUSED = GLFW_FOCUSED,										//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		AUTO_ICONIFY = GLFW_AUTO_ICONIFY,							//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		FLOATING = GLFW_FLOATING,									//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		MAXIMIZED = GLFW_MAXIMIZED,									//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		CENTER_CURSOR = GLFW_CENTER_CURSOR,							//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		TRANSPARENT_FRAMEBUFFER = GLFW_TRANSPARENT_FRAMEBUFFER,		//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		FOCUS_ON_SHOW = GLFW_FOCUS_ON_SHOW,							//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
		SCALE_TO_MONITOR = GLFW_SCALE_TO_MONITOR,					//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		STEREO = GLFW_STEREO,										//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		SRGB_CAPABLE = GLFW_SRGB_CAPABLE,							//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		DOUBLEBUFFER = GLFW_DOUBLEBUFFER,							//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE

		OPENGL_FORWARD_COMPAT = GLFW_OPENGL_FORWARD_COMPAT,			//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		OPENGL_DEBUG_CONTEXT = GLFW_OPENGL_DEBUG_CONTEXT,			//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE

		COCOA_GRAPHICS_SWITCHING = GLFW_COCOA_GRAPHICS_SWITCHING,	//	GLFW_FALSE					GLFW_TRUE or GLFW_FALSE
		COCOA_RETINA_FRAMEBUFFER = GLFW_COCOA_RETINA_FRAMEBUFFER,	//	GLFW_TRUE					GLFW_TRUE or GLFW_FALSE
	};

	enum glfwHintsInt {												//   default					    possible values
		RED_BITS = GLFW_RED_BITS,									//	8							0 to INT_MAX or GLFW_DONT_CARE
		GREEN_BITS = GLFW_GREEN_BITS,								//	8							0 to INT_MAX or GLFW_DONT_CARE
		BLUE_BITS = GLFW_BLUE_BITS,									//	8							0 to INT_MAX or GLFW_DONT_CARE
		ALPHA_BITS = GLFW_ALPHA_BITS,								//	8							0 to INT_MAX or GLFW_DONT_CARE
		DEPTH_BITS = GLFW_DEPTH_BITS,								//	24							0 to INT_MAX or GLFW_DONT_CARE
		STENCIL_BITS = GLFW_STENCIL_BITS,							//	8							0 to INT_MAX or GLFW_DONT_CARE

		ACCUM_RED_BITS = GLFW_ACCUM_RED_BITS,						//	0							0 to INT_MAX or GLFW_DONT_CARE
		ACCUM_GREEN_BITS = GLFW_ACCUM_GREEN_BITS,					//	0							0 to INT_MAX or GLFW_DONT_CARE
		ACCUM_BLUE_BITS = GLFW_ACCUM_BLUE_BITS,						//	0							0 to INT_MAX or GLFW_DONT_CARE
		ACCUM_ALPHA_BITS = GLFW_ACCUM_ALPHA_BITS,					//	0							0 to INT_MAX or GLFW_DONT_CARE

		AUX_BUFFERS = GLFW_AUX_BUFFERS,								//	0							0 to INT_MAX or GLFW_DONT_CARE
		SAMPLES = GLFW_SAMPLES,										//	0							0 to INT_MAX or GLFW_DONT_CARE
		REFRESH_RATE = GLFW_REFRESH_RATE,							//	GLFW_DONT_CARE				0 to INT_MAX or GLFW_DONT_CARE
		CLIENT_API = GLFW_CLIENT_API,								//	GLFW_OPENGL_API				GLFW_OPENGL_API, GLFW_OPENGL_ES_API or GLFW_NO_API

		CONTEXT_CREATION_API = GLFW_CONTEXT_CREATION_API,			//	GLFW_NATIVE_CONTEXT_API		GLFW_NATIVE_CONTEXT_API, GLFW_EGL_CONTEXT_API or GLFW_OSMESA_CONTEXT_API
		CONTEXT_VERSION_MAJOR = GLFW_CONTEXT_VERSION_MAJOR,			//	1							Any valid major version number of the chosen client API
		CONTEXT_VERSION_MINOR = GLFW_CONTEXT_VERSION_MINOR,			//	0							Any valid minor version number of the chosen client API
		CONTEXT_ROBUSTNESS = GLFW_CONTEXT_ROBUSTNESS,				//	GLFW_NO_ROBUSTNESS			GLFW_NO_ROBUSTNESS, GLFW_NO_RESET_NOTIFICATION or GLFW_LOSE_CONTEXT_ON_RESET
		CONTEXT_RELEASE_BEHAVIOR = GLFW_CONTEXT_RELEASE_BEHAVIOR,	//	GLFW_ANY_RELEASE_BEHAVIOR	GLFW_ANY_RELEASE_BEHAVIOR, GLFW_RELEASE_BEHAVIOR_FLUSH or GLFW_RELEASE_BEHAVIOR_NONE
		OPENGL_PROFILE = GLFW_OPENGL_PROFILE,						//	GLFW_OPENGL_ANY_PROFILE		GLFW_OPENGL_ANY_PROFILE, GLFW_OPENGL_COMPAT_PROFILE or GLFW_OPENGL_CORE_PROFILE
	};

	enum glfwHintsString {											//   default					    possible values
		COCOA_FRAME_NAME = GLFW_COCOA_FRAME_NAME,					//	""							A UTF - 8 encoded frame autosave name
		X11_CLASS_NAME = GLFW_X11_CLASS_NAME,						//	""							An ASCII encoded WM_CLASS class name
		X11_INSTANCE_NAME = GLFW_X11_INSTANCE_NAME,					//	""							An ASCII encoded WM_CLASS instance name
	};

	enum CursorType {
		HIDDEN = GLFW_CURSOR_HIDDEN,
		DISABLED = GLFW_CURSOR_DISABLED,
		NORMAL = GLFW_CURSOR_NORMAL,
	};

	struct WindowHints {
		WindowHints(std::initializer_list<std::pair<glfwHintsBool, bool>> boolsList = {}, std::initializer_list<std::pair<glfwHintsInt, int>> intsList = {}, std::initializer_list<std::pair<glfwHintsString, std::string>> stringsList = {}) {
			for (auto& [type, val] : boolsList) {
				bools[type] = val;
			}

			for (auto& [type, val] : intsList) {
				ints[type] = val;
			}

			for (auto& [type, val] : stringsList) {
				strings[type] = val;
			}
		}

		std::map<glfwHintsBool, bool> bools{
#ifdef __APPLE__
			{OPENGL_FORWARD_COMPAT, true}
#endif
		};

		std::map<glfwHintsInt, int> ints{
			{ SFE::Render::CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ },
			{ SFE::Render::CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN },
			{ SFE::Render::OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE },
		};

		std::map<glfwHintsString, std::string> strings;

		void apply();
	};
	
	class Window {
	public:
		Window(const Window& other) = delete;
		Window(Window&& other) noexcept = delete;
		Window& operator=(const Window& other) = delete;
		Window& operator=(Window&& other) noexcept = delete;

		Window() = default;
		~Window();

		Window(int w, int h, const std::string& title, GLFWwindow* share, WindowHints hints = {});

		void create(int w, int h, const std::string& title, GLFWwindow* share, WindowHints hints = {});
		GLFWwindow* getWindow() const { return mWindow; }
		bool isClosing() const;
		void swapBuffers() const;
		void setInputMode(CursorType type);
		void close() const;

		std::function<void(int, int)> onFramebufferResize = {};
		std::function<void(int, int)> onPosChange = {};

		std::function<void(int, int, int, int)> keyCallback = {};
		std::function<void(int, int, int)> mouseButtonCallback = {};
		std::function<void(double, double)> cursorPosCallback = {};
		std::function<void(double, double)> scrollCallback = {};

		std::function<void()> onCloseCallback = {};

		CoreModule::ViewportData& getScreenData() { return data; }
	private:

		CoreModule::ViewportData data;

		GLFWwindow* mWindow = nullptr;
	};

	static inline std::unordered_map<GLFWwindow*, Window*> windows;

	class WindowSystem : public Singleton<WindowSystem> {
	public:
		~WindowSystem() override;
		void init() override;

		Window* createWindow(const std::string& windowName, GLFWwindow* share, int w, int h, WindowHints hints = {});
		void deleteWindow(const std::string& windowName);

		static Window* getCurrentContext() { return contextStack.empty() ? nullptr : contextStack.top(); }

		static void pushContext(Window* window) {
			if (window) {
				glfwMakeContextCurrent(window->getWindow());
			}

			contextStack.push(window);
		}

		static void popContext() {
			if (contextStack.empty()) {
				return;
			}

			contextStack.pop();
			if (!contextStack.empty()) {
				if (contextStack.top()) {
					glfwMakeContextCurrent(contextStack.top()->getWindow());
				}
			}
			else {
				glfwMakeContextCurrent(nullptr);
			}
		}

	private:
		thread_local static inline std::stack<Window*> contextStack;

		bool mInited = false;

		std::unordered_map<std::string, Window*> mWindows;
	};
}

