#include "Window.h"

#include "logsModule/logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace SFE::Render {
	void errorCallback(int error, const char* description) {
		LogsModule::Logger::LOG_ERROR("GLFW Error: %d, %s\n", error, description);
	}

	WindowSystem::~WindowSystem() {
		for (auto& [title, window] : mWindows) {
			delete window;
		}

		mWindows.clear();
	}

	void WindowSystem::init() {
		if (mInited) {
			return;
		}
		mInited = true;

		if (!glfwInit()) {
			return;
		}

		glfwSetErrorCallback(errorCallback);
	}

	Window* WindowSystem::createWindow(const std::string& windowName, GLFWwindow* share, int w, int h, const WindowHints& hints) {
		const auto it = mWindows.find(windowName);
		if (it != mWindows.end()) {
			return it->second;
		}

		const auto window = new Window(w, h, windowName, share, hints);
		mWindows[windowName] = window;

		return window;
	}

	void WindowSystem::deleteWindow(const std::string& windowName) {
		const auto it = mWindows.find(windowName);
		if (it != mWindows.end()) {
			delete it->second;
			mWindows.erase(it);
		}
	}

	void WindowHints::apply() {
		for (auto& [hint, value] : bools) {
			glfwWindowHint(hint, value);
		}

		for (auto& [hint, value] : ints) {
			glfwWindowHint(hint, value);
		}

		for (auto& [hint, value] : strings) {
			glfwWindowHintString(hint, value.c_str());
		}
	}

	Window::~Window() {
		glfwDestroyWindow(mWindow);
		windows.erase(mWindow);
	}

	Window::Window(int w, int h, const std::string& title, GLFWwindow* share, const WindowHints& hints) {
		create(w, h, title, share, hints);
	}

	void Window::create(int w, int h, const std::string& title, GLFWwindow* share, WindowHints hints) {
		data.width = w;
		data.height = h;

		hints.apply();
		//glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
		mWindow = glfwCreateWindow(w, h, title.c_str(), nullptr, share);
		if (!mWindow) {
			LogsModule::Logger::LOG_FATAL(false, "Failed to create glfw window \"%s\"", title.c_str());
			return;
		}
		windows[mWindow] = this;
		
		glfwMakeContextCurrent(mWindow);

		glfwGetWindowPos(mWindow, &data.posX, &data.posY);

		glfwGetWindowSize(mWindow, &data.width, &data.height);
		glfwGetWindowContentScale(mWindow, &data.renderScaleW, &data.renderScaleH);
		glfwGetFramebufferSize(mWindow, &data.backbufferW, &data.backbufferH);
		// Internal render resolution stays at requested size (matches editor shared-context window).
		data.renderW = w;
		data.renderH = h;
		data.width = w;
		data.height = h;
		glfwDefaultWindowHints();

		glfwSetWindowPosCallback(mWindow, [](GLFWwindow* window, int x, int y) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					win->data.posX = x;
					win->data.posY = y;
					if (win->onPosChange) {
						win->onPosChange(x, y);
					}
				}
			}
		});

		glfwSetFramebufferSizeCallback(mWindow, [](GLFWwindow* window, int width, int height) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					win->data.backbufferW = width;
					win->data.backbufferH = height;
					if (win->onFramebufferResize) {
						win->onFramebufferResize(width, height);
					}
				}
			}
		});

		glfwSetKeyCallback(mWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					if (windows[window]->keyCallback) {
						windows[window]->keyCallback(key, scancode, action, mods);
					}
				}
			}
		});

		glfwSetMouseButtonCallback(mWindow, [](GLFWwindow* window, int btn, int act, int mode) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					if (windows[window]->mouseButtonCallback) {
						windows[window]->mouseButtonCallback(btn, act, mode);
					}
				}
			}
		});

		glfwSetCursorPosCallback(mWindow, [](GLFWwindow* window, double xPos, double yPos) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					if (windows[window]->cursorPosCallback) {
						windows[window]->cursorPosCallback(xPos, yPos);
					}
				}
			}
		});

		glfwSetScrollCallback(mWindow, [](GLFWwindow* window, double xOffset, double yOffset) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					if (windows[window]->scrollCallback) {
						windows[window]->scrollCallback(xOffset, yOffset);
					}
				}
			}
		});

		glfwSetWindowCloseCallback(mWindow, [](GLFWwindow* window) {
			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					windows.erase(window);
					if (win->onCloseCallback) {
						win->onCloseCallback();
					}
				}
			}
		});
	}

	bool Window::isClosing() const {
		if (!mWindow) {
			return true;
		}
		return glfwWindowShouldClose(mWindow);
	}

	bool Window::isVisible() const {
		return mWindow && glfwGetWindowAttrib(mWindow, GLFW_VISIBLE);
	}

	void Window::show() const {
		if (mWindow) {
			glfwShowWindow(mWindow);
		}
	}

	void Window::hide() const {
		if (mWindow) {
			glfwHideWindow(mWindow);
		}
	}

	void Window::swapBuffers() const {
		if (!mWindow) {
			return;
		}
		glfwSwapBuffers(mWindow);
	}

	void Window::setInputMode(CursorType type) {
		glfwSetInputMode(mWindow, GLFW_CURSOR, type);
	}

	void Window::close() const {
		if (isClosing()) {
			return;
		}

		glfwSetWindowShouldClose(mWindow, true);
	}

#ifdef _WIN32
	void* Window::getNativeHandle() const
	{
		return mWindow ? reinterpret_cast<void*>(glfwGetWin32Window(mWindow)) : nullptr;
	}

	bool Window::embedInHost(void* ownerHwnd, int screenX, int screenY, int w, int h)
	{
		if (!mWindow || w <= 0 || h <= 0) {
			return false;
		}
		HWND hwnd = glfwGetWin32Window(mWindow);
		if (!hwnd || !IsWindow(hwnd)) {
			return false;
		}
		if (ownerHwnd) {
			HWND owner = static_cast<HWND>(ownerHwnd);
			if (!IsWindow(owner)) {
				return false;
			}
		}

		mEmbeddedInHost = true;
		SetWindowPos(hwnd, HWND_TOP, screenX, screenY, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
		ShowWindow(hwnd, SW_SHOWNA);
		return true;
	}

	void Window::refreshEmbeddedOverlay(void* ownerHwnd, int screenX, int screenY, int w, int h) const
	{
		if (!mEmbeddedInHost || !mWindow || w <= 0 || h <= 0) {
			return;
		}
		HWND hwnd = glfwGetWin32Window(mWindow);
		if (!hwnd || !IsWindow(hwnd)) {
			return;
		}
		if (ownerHwnd) {
			HWND owner = static_cast<HWND>(ownerHwnd);
			if (!IsWindow(owner)) {
				return;
			}
		}
		SetWindowPos(hwnd, HWND_TOP, screenX, screenY, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
	}

	void Window::detachFromHost()
	{
		if (!mEmbeddedInHost || !mWindow) {
			return;
		}

		glfwHideWindow(mWindow);
		mEmbeddedInHost = false;
	}
#endif
}
