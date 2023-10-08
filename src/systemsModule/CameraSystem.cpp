#include "CameraSystem.h"

#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "..\ecss\Registry.h"

namespace Engine::SystemsModule {
	CameraSystem::CameraSystem() {
		auto aspect = static_cast<float>(RenderModule::Renderer::SCR_WIDTH) / static_cast<float>(RenderModule::Renderer::SCR_HEIGHT);
		mDefaultCamera = ECSHandler::registry()->takeEntity();

		auto transform = ECSHandler::registry()->addComponent<TransformComponent>(mDefaultCamera);
		transform->setPos({ 28.f, 218.f, 265.f });
		transform->setRotate({ -30.5f, 0.8f, 0.0f });
		ECSHandler::registry()->addComponent<CameraComponent>(mDefaultCamera, 45.f, aspect, 0.1f, 5000.f);
		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			if (type == Engine::CoreModule::InputEventType::PRESS) {
				isPressed[key] = true;
			}
			else if (type == Engine::CoreModule::InputEventType::RELEASE) {
				isPressed[key] = false;
			}
		};

	}

	void CameraSystem::update(float_t dt) {
		auto cam = getCurrentCamera();
		if (isPressed[CoreModule::InputKey::KEY_W]) {
			processKeyboard(cam, FORWARD, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_S]) {
			processKeyboard(cam, BACKWARD, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_A]) {
			processKeyboard(cam, LEFT, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_D]) {
			processKeyboard(cam, RIGHT, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_SPACE]) {
			processKeyboard(cam, TOP, dt);
		}
		if (isPressed[CoreModule::InputKey::KEY_LEFT_SHIFT]) {
			processKeyboard(cam, BOTTOM, dt);
		}
	}

	void CameraSystem::setCurrentCamera(const ecss::EntityHandle& camera) {
		mCurrentCamera = camera;
	}

	const ecss::EntityHandle& CameraSystem::getCurrentCamera() const {
		if (mCurrentCamera) {
			return mCurrentCamera;
		}

		return mDefaultCamera;
	}

	void CameraSystem::processKeyboard(const ecss::EntityHandle& camera, Camera_Movement direction, float deltaTime) {
		if (!camera) {
			return;
		}

		auto tc = ECSHandler::registry()->getComponent<TransformComponent>(camera);

		const float velocity = MovementSpeed * deltaTime;

		glm::vec3 dif = {};
		if (direction == FORWARD) {
			dif += tc->getForward() * velocity;
		}

		if (direction == BACKWARD) {
			dif -= tc->getForward() * velocity;
		}

		if (direction == LEFT) {
			dif -= tc->getRight() * velocity;
		}

		if (direction == RIGHT) {
			dif += tc->getRight() * velocity;
		}

		if (direction == TOP) {
			dif += glm::vec3{0.f, 1.f, 0.f} *velocity;
		}

		if (direction == BOTTOM) {
			dif -= glm::vec3{0.f, 1.f, 0.f} *velocity;
		}


		tc->setPos(tc->getPos() + dif);
		tc->reloadTransform();
	}


	void CameraSystem::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
		if (!processMouse) {
			return;
		}
		xoffset *= MouseSensitivity;
		yoffset *= MouseSensitivity;
		auto tc = ECSHandler::registry()->getComponent<TransformComponent>(getCurrentCamera());
		if (!tc) {
			return;
		}
		auto Pitch = tc->getRotate().x;
		auto Yaw = tc->getRotate().y;
		Yaw -= xoffset;

		Pitch += yoffset;

		if (constrainPitch) {
			Pitch = std::min(std::max(Pitch, -90.f), 90.f);
		}

		tc->setRotate({ Pitch, Yaw, 0.f });
		tc->reloadTransform();
	}
	void CameraSystem::ProcessMouseScroll(float yoffset) {
		auto fov = ECSHandler::registry()->getComponent<CameraComponent>(getCurrentCamera())->getProjection().getFOV();
		fov -= yoffset;

		ECSHandler::registry()->getComponent<CameraComponent>(getCurrentCamera())->getProjection().setFOV(std::min(std::max(1.f, fov), 90.f));
	}
}