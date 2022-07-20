#include "InputHandler.h"

#include "Engine.h"

using namespace GameEngine::CoreModule;

void InputHandler::processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		Engine::getInstance()->getCamera().ProcessKeyboard(FORWARD, Engine::getInstance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		Engine::getInstance()->getCamera().ProcessKeyboard(BACKWARD, Engine::getInstance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		Engine::getInstance()->getCamera().ProcessKeyboard(LEFT, Engine::getInstance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		Engine::getInstance()->getCamera().ProcessKeyboard(RIGHT, Engine::getInstance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		Engine::getInstance()->getCamera().ProcessKeyboard(TOP, Engine::getInstance()->getDeltaTime());
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		Engine::getInstance()->getCamera().ProcessKeyboard(BOTTOM, Engine::getInstance()->getDeltaTime());
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

	Engine::getInstance()->getCamera().ProcessMouseMovement(xoffset, yoffset);
}

void InputHandler::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	Engine::getInstance()->getCamera().ProcessMouseScroll(static_cast<float>(yoffset));
}

void InputHandler::mouseBtnInput(GLFWwindow* w, int btn, int act, int mode) {
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE && act == GLFW_PRESS) {
		Engine::getInstance()->getCamera().processMouse = true;
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE && act == GLFW_RELEASE) {
		Engine::getInstance()->getCamera().processMouse = false;
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	
}

void InputHandler::init() {
	glfwSetInputMode(Engine::getInstance()->getMainWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwSetMouseButtonCallback(Engine::getInstance()->getMainWindow(), &InputHandler::mouseBtnInput);
	glfwSetCursorPosCallback(Engine::getInstance()->getMainWindow(), &InputHandler::mouseCallback);
	glfwSetScrollCallback(Engine::getInstance()->getMainWindow(), &InputHandler::scrollCallback);	
}