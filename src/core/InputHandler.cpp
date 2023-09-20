#include "InputHandler.h"

#include "Camera.h"
#include "Engine.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"
#include "imgui.h"
#include "imgui_internal.h"

using namespace Engine::CoreModule;

void InputProvider::subscribe(InputObserver* observer) {
	auto it = std::find(mKeyObservers.cbegin(), mKeyObservers.cend(), observer);
	if (it == mKeyObservers.cend()) {
		mKeyObservers.push_back(observer);
	}
}

void InputProvider::unsubscribe(InputObserver* observer) {
	erase_if(mKeyObservers, [observer](auto obs) {
		return obs == observer;
	});

	if (mKeyObservers.empty()) {
		terminate();
	}
}

void InputProvider::fireEvent(InputKey key, InputEventType type) {
	if (ImGui::GetCurrentContext()->IO.WantCaptureKeyboard) {
		return;
	}

	for (const auto observer : mKeyObservers) {
		observer->onKeyEvent(key, type);
	}
}

void InputHandler::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	InputProvider::instance()->fireEvent(static_cast<InputKey>(key), static_cast<InputEventType>(action));
}

void InputHandler::mouseCallback(GLFWwindow* window, double xposIn, double yposIn) {
	if (ImGui::GetCurrentContext()->IO.WantCaptureMouse) {
		return;
	}
	const auto xpos = static_cast<float>(xposIn);
	const auto ypos = static_cast<float>(yposIn);

	static auto lastX = xpos;
	static auto lastY = ypos;

	auto xoffset = xpos - lastX;
	auto yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	ecsModule::ECSHandler::systemManagerInstance()->getSystem<SystemsModule::CameraSystem>()->getCurrentCamera()->ProcessMouseMovement(xoffset, yoffset);
}

void InputHandler::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	if (ImGui::GetCurrentContext()->IO.WantCaptureMouse) {
		return;
	}
	ecsModule::ECSHandler::systemManagerInstance()->getSystem<SystemsModule::CameraSystem>()->getCurrentCamera()->ProcessMouseScroll(static_cast<float>(yoffset));
}

void InputHandler::mouseBtnInput(GLFWwindow* w, int btn, int act, int mode) {
	if (ImGui::GetCurrentContext()->IO.WantCaptureMouse) {
		return;
	}
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE && act == GLFW_PRESS) {
		ecsModule::ECSHandler::systemManagerInstance()->getSystem<SystemsModule::CameraSystem>()->getCurrentCamera()->processMouse = true;
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	if (btn == GLFW_MOUSE_BUTTON_MIDDLE && act == GLFW_RELEASE) {
		ecsModule::ECSHandler::systemManagerInstance()->getSystem<SystemsModule::CameraSystem>()->getCurrentCamera()->processMouse = false;
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

}

void InputHandler::init() {
	glfwSetInputMode(UnnamedEngine::instance()->getMainWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwSetKeyCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::keyCallback);
	glfwSetMouseButtonCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::mouseBtnInput);
	glfwSetCursorPosCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::mouseCallback);
	glfwSetScrollCallback(UnnamedEngine::instance()->getMainWindow(), &InputHandler::scrollCallback);
}