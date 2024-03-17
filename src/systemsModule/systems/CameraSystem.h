#pragma once

#include "core/InputHandler.h"
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	enum CameraMovement {
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		TOP,
		BOTTOM
	};


	class CameraSystem : public ecss::System, CoreModule::InputObserver {
	public:
		CameraSystem();
		void update(float_t dt) override;

		void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
		void processMouseScroll(float yoffset);

		void setCurrentCamera(ecss::EntityId camera);
		ecss::EntityId getCurrentCamera() const;

		bool processMouse = false;
		float movementSpeed = 100.f;
		float mouseSensitivity = 0.1f;
	
	private:
		void initKeyEvents();
		void processKeyboard(ecss::EntityId camera, CameraMovement direction, float deltaTime);
		ecss::EntityId mCurrentCamera = ecss::INVALID_ID;

		ecss::EntityId mDefaultCamera = ecss::INVALID_ID;

		std::map<CoreModule::InputKey, bool> isPressed;
	};
}
