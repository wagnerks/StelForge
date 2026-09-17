#pragma once

#include <array>
#include <atomic>

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

		// While off, the keyboard and mouse no longer fly the camera: something else (CharacterSystem) drives it.
		void setUserControl(bool enabled) { mUserControl.store(enabled, std::memory_order_relaxed); }
		bool userControl() const { return mUserControl.load(std::memory_order_relaxed); }
		// Puts the current camera at `pos` turned by `rotate` (Euler degrees) and tells everyone who follows it.
		void placeCamera(const Math::Vec3& pos, const Math::Vec3& rotate);

		bool processMouse = false;
		float movementSpeed = 100.f;
		float mouseSensitivity = 0.1f;
		inline static std::atomic<bool> sReproFly{ false };
	
	private:
		void initKeyEvents();
		void refreshMouseLook();
		void processKeyboard(ecss::EntityId camera, CameraMovement direction, float deltaTime);
		bool keyDown(CoreModule::InputKey key) const;
		void setKeyDown(CoreModule::InputKey key, bool down);

		ecss::EntityId mCurrentCamera = ecss::INVALID_ID;

		ecss::EntityId mDefaultCamera = ecss::INVALID_ID;

		static constexpr int kKeyCount = 512;
		std::array<std::atomic<bool>, kKeyCount> mPressed{};
		std::atomic<bool> mmbLook{ false };
		std::atomic<bool> altLook{ false };
		std::atomic<bool> lmbDown{ false };
		std::atomic<bool> mUserControl{ true };
	};
}
