#pragma once
#include "core/Camera.h"
#include "core/InputHandler.h"
#include "ecsModule/SystemBase.h"

namespace Engine::SystemsModule {
	class CameraSystem : public ecsModule::System<CameraSystem>, CoreModule::InputObserver {
	public:
		CameraSystem();
		void preUpdate(float_t dt) override {}
		void update(float_t dt) override;
		void postUpdate(float_t dt) override {}

		void setCurrentCamera(Camera* camera);
		Camera* getCurrentCamera() const;
	private:
		void processKeyboard(Camera* camera, Camera_Movement direction, float deltaTime);
		Camera* mCurrentCamera = nullptr;

		Camera* mDefaultCamera = nullptr;

		std::map<Engine::CoreModule::InputKey, bool> isPressed;
	};
}
