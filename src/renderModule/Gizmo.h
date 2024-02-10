#pragma once
#include <array>

#include "core/InputHandler.h"
#include "ecss/Types.h"
#include "mathModule/CameraUtils.h"
#include "myPhysicsEngine/Physics.h"
#include "renderPasses/GUIPass.h"

namespace SFE::RenderModule {

	enum Axis : uint8_t {
		NONE,
		X,
		Y,
		Z,
		XY,
		XZ,
		YZ,
		XYZ//for scale
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
		std::pair<Axis, Math::Vec3> findHoveredGizmo(GizmoMode mode) const;

		ecss::EntityId mEntity = ecss::INVALID_ID;

		Axis mActiveAxis = Axis::NONE;
		Axis mHoveredAxis = Axis::NONE;

		GizmoMode mCurrentMode = GizmoMode::NONE;

		//gizmo logic variables
		Math::Vec3 mStartPos = {};
		Math::Vec3 mIntersectionStartPos = {};
		Math::Vec3 mIntersectionCurrentPos = {};

		//scale helper
		Math::Vec3 mStartScale = {};

		Math::Vec2 mMousePos = {};

		void processMove();
		void processScale();
		void processRotate();

		//draw
		mutable ecss::EntityId mQuad = ecss::INVALID_ID;
		mutable ecss::EntityId mText = ecss::INVALID_ID;

		void drawRotateGizmo(const Math::Vec3& pos);
		void drawScaleGizmo(const Math::Vec3& pos, const Math::Vec3& scale);
		void drawMoveGizmo(const Math::Vec3& pos);

		void drawPlaneQuads(const Math::Vec3& pos);
		void drawGizmoLine(Axis axis, const Math::Vec3& pos, const Math::Vec4& color, float end, GizmoMode type) const;
		void drawScaleCube(Axis axis, const Math::Vec3& pos, const Math::Vec4& color, float start) const;

		void drawAxis(const Math::Vec3& pos, Axis axis) const;
		void drawPos(const Math::Vec3& pos, const Math::Vec4& color, float size = 10.f) const;

		void drawVec3Text(const Math::Vec3& value) const;
		void drawFloatText(float value) const;
		void updateGizmosAlpha(const Math::Vec3& cameraPos, const Math::Vec3& entityPos);

		//draw settings
		const std::array<Math::Vec3, 5> mColors {
			Math::Vec3{0.f, 0.f, 0.f},//none
			Math::Vec3{0.78f, 0.25f, 0.25f},//x
			Math::Vec3{0.11f, 0.7f, 0.f},//y
			Math::Vec3{0.25f, 0.25f, 0.8f},//z
			Math::Vec3{1.f, 1.f, 1.f},//XY
		};

		float mGizmoRadius = 180.f;
		float mCommonGizmoOffset = 20.f;
		float mGizmoStartRadius = 10.f;

		const float mTipRadius = 20.f;
		const float mTipHeight = 30.f;

		const float mNotHoveredAlpha = 0.5f;
		const float mHoveredAlpha = 0.95f;
		const float mHintAlpha = 0.6f;

		float mPlaneQaudOffset = 40.f;
		float mPlaneQaudSize = 25.f;

		Math::Vec4 mInactiveColor = { 1.f,1.f,1.f,0.5f };

		float mPlaneVisibilityKoef = 0.05f;
		float mAxisVisibilityKoef = 0.2f;

		//draw utils
		float mScaleCoef = 1.f;
		float mScaleFactor = 0.0005f;
		
		Math::Vec3 mAlpha = {};
		Math::Vec3 mAlphaPlanes = {};
		float mAlphaCommonGizmo = 0.f;


		//constant helpers
		static bool isLineAxis(Axis axis) { return axis == X || axis == Y || axis == Z; }
		static Math::Vec3 findIntersectionRayWithPlane(const Math::Ray& ray, const PhysicsEngine::Triangle& plane);
		static PhysicsEngine::Triangle getMouseRayPlane(const Math::Ray& ray, Axis plane, TransformComponent* cameraTransform, const Math::Vec3& initialPos = { 0.f });
		static PhysicsEngine::Triangle getCamPlane(Axis plane, TransformComponent* cameraTransform, const Math::Vec3& initialPos = {0.f});

		const std::array<Axis, 4> mAxisPlane {
			NONE,
			YZ,
			XZ,
			XY
		};

		const std::array<Math::Vec3, 8> mAxisDirection {
			Math::Vec3{0.f, 0.f, 0.f},//none
			Math::Vec3{1.f, 0.f, 0.f},//x
			Math::Vec3{0.f, 1.f, 0.f},//y
			Math::Vec3{0.f, 0.f, 1.f},//z

			Math::Vec3{0.707f, 0.707f, 0.f},//xy
			Math::Vec3{0.707f, 0.f, 0.707f},//xz
			Math::Vec3{0.f, 0.707f, 0.707f},//yz

			Math::Vec3{1.f, 1.f, 1.f},//xyz
		};

		const std::array<Math::Vec3, 7> mAxisDirectionInv {
			Math::Vec3{0.f, 0.f, 0.f},//none
			Math::Vec3{0.f, 1.f, 1.f},//x
			Math::Vec3{1.f, 0.f, 1.f},//y
			Math::Vec3{1.f, 1.f, 0.f},//z

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
	};
}


