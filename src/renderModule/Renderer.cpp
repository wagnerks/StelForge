#include "Renderer.h"

#include <deque>

#include "core/Engine.h"
#include "logsModule/logger.h"

#include "gtc/random.hpp"

#include "propertiesModule/PropertiesSystem.h"

constexpr int GLFW_CONTEXT_VER_MAJ = 4;
constexpr int GLFW_CONTEXT_VER_MIN = 6;


using namespace Engine;
using namespace ::Engine::RenderModule;
using namespace ::Engine::CoreModule;


Renderer::~Renderer() {
	delete mBatcher;
	glfwTerminate();
}

void Renderer::draw() {
	RenderModule::Renderer::mDrawCallsCount = 0;
	RenderModule::Renderer::mDrawVerticesCount = 0;
}

void Renderer::postDraw() {
	glfwSwapBuffers(UnnamedEngine::instance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {

	auto root = PropertiesModule::PropertiesSystem::loadScene("shadowsTest.json");

	mBatcher = new Batcher();
}


void Renderer::drawArrays(GLenum mode, GLsizei size, GLint first) {
	glDrawArrays(mode, first, size);
	mDrawCallsCount++;
	mDrawVerticesCount += size;
}

void Renderer::drawElements(GLenum mode, GLsizei size, GLenum type, const void* place) {
	glDrawElements(mode, size, type, place);
	mDrawCallsCount++;
	mDrawVerticesCount += size;
}

void Renderer::drawElementsInstanced(GLenum mode, GLsizei size, GLenum type, GLsizei instancesCount, const void* place) {
	glDrawElementsInstanced(mode, size, type, place, instancesCount);
	mDrawCallsCount++;
	mDrawVerticesCount += size * instancesCount;
}

void Renderer::drawArraysInstancing(GLenum mode, GLsizei size, GLsizei instancesCount, GLint first) {
	glDrawArraysInstanced(mode, first, size, instancesCount);
	mDrawCallsCount++;
	mDrawVerticesCount += size * instancesCount;
}

GLFWwindow* Renderer::initGLFW() {
	if (mGLFWInited) {
		assert(false && "GLFW Already inited");
		return nullptr;
	}
	mGLFWInited = true;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	auto window = glfwCreateWindow(Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, "GameEngine", nullptr, nullptr);
	if (window == nullptr) {
		LogsModule::Logger::LOG_ERROR("Failed to create GLFW window");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		glViewport(0, 0, width, height);
		Renderer::SCR_WIDTH = width;
		Renderer::SCR_HEIGHT = height;
	});

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(0);


	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	//glDepthFunc(GL_LESS);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);
	glClearDepth(50000.0);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);


	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
