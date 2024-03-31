#include "Window.h"

#include "logsModule/logger.h"

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

	Window* WindowSystem::createWindow(const std::string& windowName, GLFWwindow* share, int w, int h, WindowHints hints) {
		const auto it = mWindows.find(windowName);
		if (it != mWindows.end()) {
			return it->second;
		}

		hints.apply();
		
		const auto window = new Window(w, h, windowName, share);
		mWindows[windowName] = window;

		glfwDefaultWindowHints();

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

	Window::Window(int w, int h, const std::string& title, GLFWwindow* share, WindowHints hints) {
		create(w, h, title, share, hints);
	}

	void Window::create(int w, int h, const std::string& title, GLFWwindow* share, WindowHints hints) {
		data.width = w;
		data.height = h;

		hints.apply();
		mWindow = glfwCreateWindow(w, h, title.c_str(), nullptr, share);
		if (!mWindow) {
			LogsModule::Logger::LOG_FATAL(false, "Failed to create glfw window \"%s\"", title.c_str());
			return;
		}
		windows[mWindow] = this;

		glfwGetWindowPos(mWindow, &data.posX, &data.posY);

		glfwGetWindowSize(mWindow, &data.width, &data.height);
		glfwGetWindowContentScale(mWindow, &data.renderScaleW, &data.renderScaleH);
		glfwGetFramebufferSize(mWindow, &data.renderW, &data.renderH);

		data.setSize(data.width, data.height);

		if (!share) {
			glfwMakeContextCurrent(mWindow);
			LogsModule::Logger::LOG_FATAL(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "Failed to initialize GLAD");
			glfwMakeContextCurrent(nullptr);
		}

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
			glfwSetWindowSize(window, width, height);

			if (windows.contains(window)) {
				if (const auto win = windows[window]) {
					win->data.setSize(width, height);
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
		return glfwWindowShouldClose(mWindow);
	}

	void Window::swapBuffers() const {
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
}
