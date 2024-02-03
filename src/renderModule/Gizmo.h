#pragma once
#include <array>

#include "Utils.h"
#include "core/InputHandler.h"
#include "ecss/Types.h"
#include "mathModule/CameraUtils.h"
#include "myPhysicsEngine/Physics.h"

namespace SFE::RenderModule {

	enum Axis : uint8_t {
		NONE,
		X,
		Y,
		Z,
		XY,
		XZ,
		YZ
	};

	enum class GizmoMode : uint8_t {
		NONE,
		MOVE,
		ROTATE,
		SCALE
	};

	class Gizmo : public CoreModule::InputObserver {
	public:
		Gizmo();
		void setEntity(ecss::EntityId newId) { mEntity = newId; }
		void update();

		void setMode(GizmoMode mode) {
			mCurrentMode = mode;
		}
	private:

		Math::Vec3 findIntersecRayWithPlane(const Math::Ray& ray, const PhysicsEngine::Triangle& plane);
		PhysicsEngine::Triangle getCamPlane(const Math::Ray& ray, Axis plane, TransformComponent* cameraTransform);
		ecss::EntityId mEntity = ecss::INVALID_ID;

		std::pair<Axis, Math::Vec3> getHoveredGizmo(GizmoMode mode);
		void drawRotateGizmo(const Math::Vec3& pos);
		void drawScaleGizmo(const Math::Vec3& pos);
		void drawPosGizmo(const Math::Vec3& pos);

		Axis mActiveAxis = Axis::NONE;
		Axis mHoveredAxis = Axis::NONE;

		GizmoMode mCurrentMode = GizmoMode::NONE;
		Math::Vec3 mPrevPos = {};
		Math::Vec3 mPrevPosPos = {};
		Math::Vec3 mAnglePos = {};
		Math::Vec3 mPrevScale = {};

		Math::Vec3 mAlpha = {};
		Math::Vec2 mMousePos = {};

		const std::array<Axis, 4> mAxisPlane {
			NONE,
			YZ,
			XZ,
			XY
		};

		const std::array<Math::Vec3, 4> mColors {
			Math::Vec3{0.f,0.f,0.f},//none
			Math::Vec3{1.f,0.f,0.f},//x
			Math::Vec3{0.f,1.f,0.f},//y
			Math::Vec3{0.f,0.f,1.f},//z
		};

		const std::array<Math::Vec3, 7> mAxisDirection {
			Math::Vec3{0.f, 0.f, 0.f},//none
			Math::Vec3{1.f, 0.f, 0.f},//x
			Math::Vec3{0.f, 1.f, 0.f},//y
			Math::Vec3{0.f, 0.f, 1.f},//z

			Math::Vec3{0.707f, 0.707f, 0.f},//xy
			Math::Vec3{0.707f, 0.f, 0.707f},//xz
			Math::Vec3{0.f, 0.707f, 0.707f},//yz
		};


		const std::array<PhysicsEngine::Triangle, 7> mPlanes {
			PhysicsEngine::Triangle{},//none
			PhysicsEngine::Triangle{Math::Vec3(0.f, 0.f, 0.f), mAxisDirection[Y], mAxisDirection[Z]}, //X
			PhysicsEngine::Triangle{Math::Vec3(0.f, 0.f, 0.f), mAxisDirection[X], mAxisDirection[Z]}, //Y
			PhysicsEngine::Triangle{Math::Vec3(0.f, 0.f, 0.f), mAxisDirection[X], mAxisDirection[Y]}, //Z

			PhysicsEngine::Triangle{Math::Vec3(0.f, 0.f, 0.f), mAxisDirection[X], mAxisDirection[Y]}, //XY
			PhysicsEngine::Triangle{Math::Vec3(0.f, 0.f, 0.f), mAxisDirection[X], mAxisDirection[Z]}, //XZ
			PhysicsEngine::Triangle{Math::Vec3(0.f, 0.f, 0.f), mAxisDirection[Y], mAxisDirection[Z]}, //YZ

		};
		std::vector<SFE::RenderModule::Utils::Triangle> mCone;
	};
}


