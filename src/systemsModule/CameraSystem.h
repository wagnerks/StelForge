#pragma once

#include "core/InputHandler.h"
#include "ecss/base/EntityHandle.h"
#include "SystemBase.h"

namespace Engine::SystemsModule {
	enum Camera_Movement {
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		TOP,
		BOTTOM
	};


	class CameraSystem : public ecss::System<CameraSystem>, CoreModule::InputObserver {
	public:
		CameraSystem();
		void preUpdate(float_t dt) override {}
		void update(float_t dt) override;
		void postUpdate(float_t dt) override {}

		void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true);
		void ProcessMouseScroll(float yoffset);

		void setCurrentCamera(const ecss::EntityHandle& camera);
		const ecss::EntityHandle& getCurrentCamera() const;
		bool processMouse = false;
		float MovementSpeed = 10.f;
		float MouseSensitivity = 0.1f;
	private:
		void processKeyboard(const ecss::EntityHandle& camera, Camera_Movement direction, float deltaTime);
		ecss::EntityHandle mCurrentCamera;

		ecss::EntityHandle mDefaultCamera;

		std::map<Engine::CoreModule::InputKey, bool> isPressed;

	};
}
