#include "Renderer.h"

#include <deque>
#include <random>
#include <stb_image.h>


#include "core/Engine.h"
#include "multithreading/ThreadPool.h"

#include "debugModule/Benchmark.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Depth.h"
#include "glWrapper/ViewportStack.h"
#include "glWrapper/Draw.h"
#include "logsModule/logger.h"

constexpr int GLFW_CONTEXT_VER_MAJ = 4;
#ifdef __APPLE__
constexpr int GLFW_CONTEXT_VER_MIN = 1; //apple supports maximum 4.1 opengl...
#else
constexpr int GLFW_CONTEXT_VER_MIN = 6;
#endif

namespace SFE::Render {
	void errorCallback(int error, const char* description) {
		LogsModule::Logger::LOG_ERROR("GLFW Error: %s\n", description);
	}

	void Window::init() {
		if (mGLFWInited) {
			return;
		}
		mGLFWInited = true;

		if (!glfwInit()) {
			return;
		}

		glfwSetErrorCallback(errorCallback);
	}

	Window Window::createWindow(int w, int h, const std::string& title, GLFWwindow* share) {
		Window res;
		res.data.width = w;
		res.data.height = h;
		if (!glfwInit()) {
			return {};
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		glfwWindowHint(GLFW_SAMPLES, 2);

#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

		res.mWindow = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
		if (!res.mWindow) {
			glfwTerminate();
			return {};
		}

		glfwGetWindowContentScale(res.mWindow, &res.data.renderScaleW, &res.data.renderScaleH);
		glfwGetFramebufferSize(res.mWindow, &res.data.renderW, &res.data.renderH);
#ifdef __APPLE__
		res.data.width = res.data.renderW / res.data.renderScaleW;
		res.data.height = res.data.renderH / res.data.renderScaleH;
#endif

		/*if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
			LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
			glfwTerminate();
			glfwDestroyWindow(res.mWindow);
			return {};
		}*/

		return res;
	}

	Renderer::~Renderer() {
		glfwTerminate();
	}

	void Renderer::swapBuffer() {
		FUNCTION_BENCHMARK;
		glfwSwapBuffers(Engine::instance()->getMainWindow());
		glfwPollEvents();
	}

	GLFWwindow* Renderer::initGLFW() {
		if (mGLFWInited) {
			return nullptr;
		}
		mGLFWInited = true;

		Window::init();

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		glfwWindowHint(GLFW_SAMPLES, 2);

#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

		const auto window = glfwCreateWindow(Renderer::screenDrawData.width, Renderer::screenDrawData.height, "StelForge Engine", nullptr, nullptr);
		if (!window) {
			glfwTerminate();
			return nullptr;
		}

		glfwGetWindowContentScale(window, &Renderer::screenDrawData.renderScaleW, &Renderer::screenDrawData.renderScaleH);
		glfwGetFramebufferSize(window, &Renderer::screenDrawData.renderW, &Renderer::screenDrawData.renderH);
#ifdef __APPLE__
		Renderer::screenDrawData.width = Renderer::screenDrawData.renderW / Renderer::screenDrawData.renderScaleW;
		Renderer::screenDrawData.height = Renderer::screenDrawData.renderH / Renderer::screenDrawData.renderScaleH;
#endif

		glfwMakeContextCurrent(window);

		GLFWimage icons[4];
		int nrChannels;
		icons[0].pixels = stbi_load("icon256x256.png", &icons[0].width, &icons[0].height, &nrChannels, 4);
		icons[1].pixels = stbi_load("icon32x32.png", &icons[1].width, &icons[1].height, &nrChannels, 4);
		icons[2].pixels = stbi_load("icon48x48.png", &icons[2].width, &icons[2].height, &nrChannels, 4);
		icons[3].pixels = stbi_load("icon24x24.png", &icons[3].width, &icons[3].height, &nrChannels, 4);

		glfwSetWindowIcon(window, 4, icons);
		stbi_image_free(icons[0].pixels);
		stbi_image_free(icons[1].pixels);
		stbi_image_free(icons[2].pixels);
		stbi_image_free(icons[3].pixels);

		glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
			glfwSetWindowSize(window, width, height);
			GLW::ViewportStack::pop();
			GLW::ViewportStack::push({ { width, height} });
			Renderer::screenDrawData.renderW = width;
			Renderer::screenDrawData.renderH = height;
#ifdef __APPLE__
			Renderer::screenDrawData.width = Renderer::screenDrawData.renderW / Renderer::screenDrawData.renderScaleW;
			Renderer::screenDrawData.height = Renderer::screenDrawData.renderH / Renderer::screenDrawData.renderScaleH;
#else
			Renderer::screenDrawData.width = Renderer::screenDrawData.renderW;
			Renderer::screenDrawData.height = Renderer::screenDrawData.renderH;
#endif
		});

		if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
			glfwTerminate();
			glfwDestroyWindow(window);
			return nullptr;
		}

		//glfwSwapInterval(3);
		glfwSwapInterval(0);

		GLW::ViewportStack::push({ { screenDrawData.renderW, screenDrawData.renderH} });
		GLW::CapabilitiesStack<GLW::CULL_FACE>::push(true);
		GLW::CapabilitiesStack<GLW::DEPTH_TEST>::push(true);
		GLW::DepthFuncStack::push(GLW::DepthFunc::LESS);

		GLW::setDepthDistance(screenDrawData.far);

		GLW::setClearColor(0.f, 0.f, 0.f);

		LogsModule::Logger::LOG_INFO("GLFW initialized");

		return window;
	}
}
