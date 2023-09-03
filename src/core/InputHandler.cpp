#include "InputHandler.h"

#include "Camera.h"
#include "Engine.h"

using namespace Engine::CoreModule;

void InputHandler::processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		UnnamedEngine::instance()->getCamera()->ProcessKeyboard(FORWARD, UnnamedEngine::instance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		UnnamedEngine::instance()->getCamera()->ProcessKeyboard(BACKWARD, UnnamedEngine::instance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		UnnamedEngine::instance()->getCamera()->ProcessKeyboard(LEFT, UnnamedEngine::instance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		UnnamedEngine::instance()->getCamera()->ProcessKeyboard(RIGHT, UnnamedEngine::instance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		UnnamedEngine::instance()->getCamera()->ProcessKeyboard(TOP, UnnamedEngine::instance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		UnnamedEngine::instance()->getCamera()->ProcessKeyboard(BOTTOM, UnnamedEngine::instance()->getDeltaTime());
}

void InputHandler::mouseCallback(GLFWwindow* window, double xposIn, double yposIn) {
	const auto xpos = static_cast<float>(xposIn);
	const auto ypos = static_cast<float>(yposIn);

	static auto lastX = xpos;
	static auto lastY = ypos;

	auto xoffset = xpos - lastX;
	auto yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	UnnamedEngine::instance()->getCamera()->ProcessMouseMovement(xoffset, yoffset);
}

void InputHandler::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	UnnamedEngine::instance()->getCamera()->ProcessMouseScroll(static_cast<float>(yoffset));
}

void InputHandler::mouseBtnInput(GLFWwindow* w, int btn, int act, int mode) {
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE && act == GLFW_PRESS) {
		UnnamedEngine::instance()->getCamera()->processMouse = true;
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE && act == GLFW_RELEASE) {
		UnnamedEngine::instance()->getCamera()->processMouse = false;
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

}

void InputHandler::init() {
	glfwSetInputMode(UnnamedEngine::instance()->getMainWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwSetMouseButtonCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::mouseBtnInput);
	glfwSetCursorPosCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::mouseCallback);
	glfwSetScrollCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::scrollCallback);
}