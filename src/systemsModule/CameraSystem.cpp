#include "CameraSystem.h"

#include "ecsModule/EntityManager.h"

namespace Engine::SystemsModule {
	CameraSystem::CameraSystem() {
		auto aspect = static_cast<float>(RenderModule::Renderer::SCR_WIDTH) / static_cast<float>(RenderModule::Renderer::SCR_HEIGHT);
		mDefaultCamera = ecsModule::ECSHandler::entityManagerInstance()->createEntity<Camera>(45.f, aspect, 0.1f, 5000.f);

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

	void CameraSystem::setCurrentCamera(Camera* camera) {
		mCurrentCamera = camera;
	}

	Camera* CameraSystem::getCurrentCamera() const {
		if (mCurrentCamera) {
			return mCurrentCamera;
		}

		return mDefaultCamera;
	}

	void CameraSystem::processKeyboard(Camera* camera, Camera_Movement direction, float deltaTime) {
		if (!camera) {
			return;
		}

		auto tc = camera->getComponent<TransformComponent>();

		const float velocity = camera->MovementSpeed * deltaTime;

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
}

