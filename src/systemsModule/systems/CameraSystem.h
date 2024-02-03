#pragma once

#include "core/InputHandler.h"
#include "ecss/EntityHandle.h"
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

		void setCurrentCamera(const ecss::EntityHandle& camera);
		const ecss::EntityHandle& getCurrentCamera() const;

		bool processMouse = false;
		float movementSpeed = 100.f;
		float mouseSensitivity = 0.1f;
	
	private:
		void initKeyEvents();
		void processKeyboard(const ecss::EntityHandle& camera, CameraMovement direction, float deltaTime);
		ecss::EntityHandle mCurrentCamera;

		ecss::EntityHandle mDefaultCamera;

		std::map<CoreModule::InputKey, bool> isPressed;
	};
}
