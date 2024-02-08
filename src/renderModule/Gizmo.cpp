#include "Gizmo.h"

#include "imgui.h"
#include "Utils.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/TreeComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "mathModule/CameraUtils.h"
#include "myPhysicsEngine/Physics.h"
#include "systemsModule/systems/CameraSystem.h"

namespace SFE::RenderModule {
	Gizmo::Gizmo() {
		onMouseBtnEvent = [this](Math::DVec2 mousePos, CoreModule::MouseButton btn, CoreModule::InputEventType eventType) {
			if (mCurrentMode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity) || mHoveredAxis == NONE) {
				return;
			}

			if (btn == CoreModule::MouseButton::MOUSE_BUTTON_LEFT && eventType == CoreModule::InputEventType::PRESS) {
				mMousePos = Math::Vec2(mousePos);

				mActiveAxis = mHoveredAxis;

				mIntersectionStartPos = findHoveredGizmo(mCurrentMode).second;
				mIntersectionCurrentPos = mIntersectionStartPos;

				const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);

				mStartPos = transformComp->getPos(true);
				mStartScale = transformComp->getScale();
			}
			else {
				mActiveAxis = Axis::NONE;
			}
		};

		onMouseEvent = [this](Math::DVec2 mousePos, Math::DVec2 mouseOffset) {
			if (mCurrentMode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity)) {
				return;
			}

			mMousePos = { mousePos };
			if (mActiveAxis == Axis::NONE) {
				mHoveredAxis = findHoveredGizmo(mCurrentMode).first;
			}
			else {
				switch (mCurrentMode) {
				case GizmoMode::MOVE: processMove(); break;
				case GizmoMode::ROTATE: processRotate(); break;
				case GizmoMode::SCALE: processScale(); break;
				default:;
				}
			}
		};
	}

	void Gizmo::update() {
		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Gizmo mode")) {
					if (ImGui::RadioButton("None", mCurrentMode == GizmoMode::NONE)) {
						mCurrentMode = GizmoMode::NONE;
					}
					if (ImGui::RadioButton("Scale", mCurrentMode == GizmoMode::SCALE)) {
						mCurrentMode = GizmoMode::SCALE;
					}
					if (ImGui::RadioButton("Move", mCurrentMode == GizmoMode::MOVE)) {
						mCurrentMode = GizmoMode::MOVE;
					}
					if (ImGui::RadioButton("Rotate", mCurrentMode == GizmoMode::ROTATE)) {
						mCurrentMode = GizmoMode::ROTATE;
					}
					ImGui::EndMenu();
				}
			}
			ImGui::EndMainMenuBar();
		}

		if (mCurrentMode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity)) {
			return;
		}

		const auto transform = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		if (!transform) {
			return;
		}

		const auto& entityPos = transform->getPos(true);
		const auto& cameraPos = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera())->getPos(true);
		//to save perspective in case of plane moves if there is active axis - use start pos
		mScaleCoef = Math::distance(cameraPos, mActiveAxis ? mStartPos : entityPos) * mScaleFactor;

		if (mActiveAxis == Axis::NONE) {
			updateGizmosAlpha(cameraPos, entityPos);
		}

		switch (mCurrentMode) {
		case GizmoMode::MOVE: drawMoveGizmo(entityPos); break;
		case GizmoMode::ROTATE: drawRotateGizmo(entityPos); break;
		case GizmoMode::SCALE: drawScaleGizmo(entityPos, transform->getScale()); break;
		default:;
		}
	}

	//process logic
	std::pair<Axis, Math::Vec3> Gizmo::findHoveredGizmo(GizmoMode mode) const {
		if (mode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity)) {
			return {};
		}

		const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
		const auto mouseRay = Math::calcMouseRay(camera, mMousePos);

		const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		if (!transformComp) {
			return {};
		}

		const auto entityPos = transformComp->getPos(true);

		if (PhysicsEngine::Physics::distancePointToLine(entityPos, mouseRay.a, mouseRay.direction) > (mGizmoRadius + mCommonGizmoOffset + mTipRadius) * mScaleCoef) {
			return {};
		}

		const auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);

		if (mode == GizmoMode::ROTATE) {
			const auto innerCollideRadius = (mGizmoRadius - mTipRadius) * mScaleCoef;
			const auto outerCollideRadius = (mGizmoRadius + mTipRadius) * mScaleCoef;

			auto intersectionPos = findIntersectionRayWithPlane(mouseRay, mPlanes[Z] + entityPos);
			auto distance = Math::distance(entityPos, intersectionPos);
			if (distance < outerCollideRadius && distance > innerCollideRadius) {
				return { Axis::Z , intersectionPos };
			}

			intersectionPos = findIntersectionRayWithPlane(mouseRay, mPlanes[Y] + entityPos);
			distance = Math::distance(entityPos, intersectionPos);
			if (distance < outerCollideRadius && distance > innerCollideRadius) {
				return { Axis::Y , intersectionPos };
			}

			intersectionPos = findIntersectionRayWithPlane(mouseRay, mPlanes[X] + entityPos);
			distance = Math::distance(entityPos, intersectionPos);
			if (distance < outerCollideRadius && distance > innerCollideRadius) {
				return { Axis::X , intersectionPos };
			}
		}
		else {
			auto resX = findIntersectionRayWithPlane({ entityPos ,mAxisDirection[X] }, getMouseRayPlane(mouseRay, X, cameraTransform, mouseRay.a));
			auto resY = findIntersectionRayWithPlane({ entityPos ,mAxisDirection[Y] }, getMouseRayPlane(mouseRay, Y, cameraTransform, mouseRay.a));
			auto resZ = findIntersectionRayWithPlane({ entityPos ,mAxisDirection[Z] }, getMouseRayPlane(mouseRay, Z, cameraTransform, mouseRay.a));

			{
				const auto XD = PhysicsEngine::Physics::distancePointToLine(resX, mouseRay.a, mouseRay.direction);
				const auto YD = PhysicsEngine::Physics::distancePointToLine(resY, mouseRay.a, mouseRay.direction);
				const auto ZD = PhysicsEngine::Physics::distancePointToLine(resZ, mouseRay.a, mouseRay.direction);

				const float dif = mTipRadius * mScaleCoef;
				const float gizmoStart = mGizmoStartRadius * mScaleCoef;
				const float gizmoEnd = (mGizmoRadius + mTipHeight) * mScaleCoef;

				if (resX.x - entityPos.x > gizmoStart && resX.x - entityPos.x < gizmoEnd && XD < YD && XD < ZD && XD < dif) {
					return { Axis::X, resX };
				}

				if (resY.y - entityPos.y > gizmoStart && resY.y - entityPos.y < gizmoEnd && YD < XD && YD < ZD && YD < dif) {
					return { Axis::Y, resY };
				}

				if (resZ.z - entityPos.z > gizmoStart && resZ.z - entityPos.z < gizmoEnd && ZD < YD && ZD < XD && ZD < dif) {
					return { Axis::Z, resZ };
				}
			}

			const auto quadStart = mPlaneQaudOffset * mScaleCoef;
			const auto quadEnd = (mPlaneQaudOffset + mPlaneQaudSize) * mScaleCoef;

			resX = findIntersectionRayWithPlane(mouseRay, mPlanes[X] + entityPos);
			if (resX.y > entityPos.y + quadStart && resX.y < entityPos.y + quadEnd && resX.z > entityPos.z + quadStart && resX.z < entityPos.z + quadEnd) {
				return { Axis::YZ, resX };
			}

			resY = findIntersectionRayWithPlane(mouseRay, mPlanes[Y] + entityPos);
			if (resY.x > entityPos.x + quadStart && resY.x < entityPos.x + quadEnd && resY.z > entityPos.z + quadStart && resY.z < entityPos.z + quadEnd) {
				return { Axis::XZ, resY };
			}

			resZ = findIntersectionRayWithPlane(mouseRay, mPlanes[Z] + entityPos);
			if (resZ.y > entityPos.y + quadStart && resZ.y < entityPos.y + quadEnd && resZ.x > entityPos.x + quadStart && resZ.x < entityPos.x + quadEnd) {
				return { Axis::XY, resZ };
			}
		}

		if (mode == GizmoMode::SCALE || mode == GizmoMode::ROTATE) {
			const auto intersectionPos = findIntersectionRayWithPlane(mouseRay, getCamPlane(XYZ, cameraTransform, entityPos));
			const auto distance = Math::distance(entityPos, intersectionPos);
			if (distance < (mGizmoRadius + mCommonGizmoOffset + mTipRadius) * mScaleCoef && distance > (mGizmoRadius + mCommonGizmoOffset - mTipRadius) * mScaleCoef) {
				return { Axis::XYZ , intersectionPos };
			}
		}

		return{};
	}

	void Gizmo::processMove() {
		const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
		const auto mouseRay = Math::calcMouseRay(camera, mMousePos);

		const auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
		const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		const auto& entityPos = transformComp->getPos(true);

		if (isLineAxis(mActiveAxis)) {
			mIntersectionCurrentPos = findIntersectionRayWithPlane({ entityPos, mAxisDirection[mActiveAxis] }, getMouseRayPlane(mouseRay, mActiveAxis, cameraTransform, mouseRay.a));
		}
		else {
			mIntersectionCurrentPos = findIntersectionRayWithPlane(mouseRay, mPlanes[mActiveAxis] + entityPos);
		}

		auto curPos = mIntersectionCurrentPos;
		auto startPos = mIntersectionStartPos;

		mIntersectionStartPos = mIntersectionCurrentPos;

		if (const auto treeComp = ECSHandler::registry().getComponent<TreeComponent>(mEntity)) {
			if (const auto parentTransform = ECSHandler::registry().getComponent<TransformComponent>(treeComp->getParent())) {
				const auto transform = inverse(parentTransform->getTransform());

				curPos = transform * Math::Vec4(curPos, 1.f);
				startPos = transform * Math::Vec4(startPos, 1.f);
			}
		}

		transformComp->setPos(transformComp->getPos() + curPos - startPos);
	}

	void Gizmo::processScale() {
		const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
		const auto mouseRay = Math::calcMouseRay(camera, mMousePos);

		const auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
		const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		const auto& entityPos = transformComp->getPos(true);

		if (isLineAxis(mActiveAxis)) {
			mIntersectionCurrentPos = findIntersectionRayWithPlane({ entityPos, mAxisDirection[mActiveAxis] }, getMouseRayPlane(mouseRay, mActiveAxis, cameraTransform, mouseRay.a));
		}
		else if (mActiveAxis != XYZ) {
			mIntersectionCurrentPos = findIntersectionRayWithPlane(mouseRay, mPlanes[mActiveAxis] + entityPos);
		}
		else {
			mIntersectionCurrentPos = findIntersectionRayWithPlane(mouseRay, getCamPlane(XYZ, cameraTransform, entityPos));
		}

		auto scaleDir = transformComp->getQuaternion().globalToLocal(mAxisDirection[mActiveAxis]);
		scaleDir = { std::fabs(scaleDir.x), std::fabs(scaleDir.y), std::fabs(scaleDir.z) };

		const auto initialValue = Math::distance(mIntersectionStartPos, mStartPos);
		const auto currentValue = Math::distance(mIntersectionCurrentPos, mStartPos);

		const auto delta = currentValue / initialValue - 1.f;

		transformComp->setScale(mStartScale + mStartScale * scaleDir * delta);
	}

	void Gizmo::processRotate() {
		const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
		const auto mouseRay = Math::calcMouseRay(camera, mMousePos);

		const auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
		const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		auto entityPos = transformComp->getPos(true);

		auto oldPos = entityPos + Math::normalize(mIntersectionCurrentPos - entityPos) * mGizmoRadius;
		mIntersectionCurrentPos = findIntersectionRayWithPlane(mouseRay, isLineAxis(mActiveAxis) ? mPlanes[mActiveAxis] + entityPos : getCamPlane(XYZ, cameraTransform, entityPos));
		auto newPos = entityPos + Math::normalize(mIntersectionCurrentPos - entityPos) * mGizmoRadius;

		auto difAngle = Math::calcAngleOnCircle(Math::distance(oldPos, newPos), mGizmoRadius);

		if (isLineAxis(mActiveAxis)) {
			if (Math::cross(oldPos - entityPos, newPos - entityPos)[mActiveAxis - 1] <= 0.f) {
				difAngle = -difAngle;
			}
		}
		else {
			const auto camTrans = inverse(cameraTransform->getTransform());
			oldPos = camTrans * Math::Vec4(oldPos, 1.f);
			newPos = camTrans * Math::Vec4(newPos, 1.f);
			entityPos = camTrans * Math::Vec4(entityPos, 1.f);

			if (Math::cross(oldPos - entityPos, newPos - entityPos).z > 0.f) {
				difAngle = -difAngle;
			}
		}

		Math::Quaternion<float> q;
		q.eulerToQuaternion((isLineAxis(mActiveAxis) ? mAxisDirection[mActiveAxis] : cameraTransform->getForward()) * Math::degrees(difAngle));
		q = q * transformComp->getQuaternion();
		transformComp->setRotate(Math::degrees(q.toEuler()));
	}

	//draw functions
	void Gizmo::drawRotateGizmo(const Math::Vec3& pos) {
		const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
		const Math::Mat4 scale = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{mScaleCoef});
		static const std::array ROTATE_ROTATIONS {
			Math::Quaternion{1.f,0.f,0.f,0.f},
			Math::Quaternion{ 0.707f, 0.f, -0.707f, 0.f },
			Math::Quaternion{ 0.707f, 0.707f, 0.f, 0.f },
			Math::Quaternion{ 0.f, 1.f, 0.f, 0.f }
		};

		if (mActiveAxis == NONE) {
			Utils::renderCircle(pos, ROTATE_ROTATIONS[X], scale, mGizmoRadius, { mColors[Axis::X], mAlpha.x }, 64);
			Utils::renderCircle(pos, ROTATE_ROTATIONS[Y], scale, mGizmoRadius, { mColors[Axis::Y], mAlpha.y }, 64);
			Utils::renderCircle(pos, ROTATE_ROTATIONS[Z], scale, mGizmoRadius, { mColors[Axis::Z], mAlpha.z }, 64);

			Utils::renderCircle(pos, ECSHandler::registry().getComponent<TransformComponent>(camera)->getQuaternion(), scale, mGizmoRadius + mCommonGizmoOffset, {1.f, 1.f, 1.f, mAlphaCommonGizmo }, 64);
		}
		else {
			const Math::Quaternion<float>* rotation = nullptr;
			Math::Vec3 color {1.f};

			const float radius = isLineAxis(mActiveAxis) ? mGizmoRadius : mGizmoRadius + mCommonGizmoOffset;

			const auto oldPos = pos + Math::normalize(mIntersectionStartPos - pos) * radius * mScaleCoef;
			const auto newPos = pos + Math::normalize(mIntersectionCurrentPos - pos) * radius * mScaleCoef;

			float difAngle = Math::calcAngleOnCircle(Math::distance(oldPos, newPos), radius * mScaleCoef);
			float prevAngle = 0.f;

			if (isLineAxis(mActiveAxis)) {
				drawAxis(pos, mActiveAxis);
				rotation = &ROTATE_ROTATIONS[mActiveAxis];
				color = mColors[mActiveAxis];

				if (Math::cross(oldPos - pos, newPos - pos)[mActiveAxis - 1] <= 0.f) {
					difAngle = 2.0f * Math::pi<float>() - difAngle;
				}

				const Axis axis = mActiveAxis == X ? Z : mActiveAxis == Y ? X : mActiveAxis == Z ? X : NONE;
				prevAngle = Math::degrees(Math::calcAngleBetweenVectors(mIntersectionStartPos - pos, mAxisDirection[axis]));
			}
			else if (mActiveAxis == XYZ) {
				const auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
				rotation = &ECSHandler::registry().getComponent<TransformComponent>(camera)->getQuaternion();

				const auto camTrans = inverse(cameraTransform->getTransform());
				const Math::Vec3 localOldPos = camTrans * Math::Vec4(oldPos, 1.f);
				const Math::Vec3 localNewPos = camTrans * Math::Vec4(newPos, 1.f);
				const Math::Vec3 localEntityPos = camTrans * Math::Vec4(pos, 1.f);

				if (Math::cross(localOldPos - localEntityPos, localNewPos - localEntityPos).z > 0.f) {
					difAngle = 2.0f * Math::pi<float>() - difAngle;
				}

				prevAngle = -Math::degrees(Math::calcAngleBetweenVectors(localOldPos - localEntityPos, { 0.f,1.f,0.f })) + 90.f;
			}

			if (rotation) {
				Utils::renderCircle(pos, *rotation, scale, radius, { color, 1.f }, 64);

				Utils::renderCircleFilled(pos, *rotation, scale, radius, { color, 0.1f }, 32, prevAngle, -Math::degrees(difAngle));

				//segment lines
				Utils::renderLine(pos, oldPos, { color, mHintAlpha }, 1);
				Utils::renderLine(pos, newPos, { color, mHintAlpha }, 3);

				drawFloatText(Math::degrees(difAngle));
			}
		}
	}

	void Gizmo::drawScaleGizmo(const Math::Vec3& pos, const Math::Vec3& scale) {
		if (mActiveAxis == NONE) {
			drawPos(pos, { 0.f,0.f,0.f, mHintAlpha });
			drawPlaneQuads(pos);

			drawGizmoLine(X, pos, { mColors[X], mAlpha.x }, mGizmoRadius, GizmoMode::SCALE);
			drawGizmoLine(Y, pos, { mColors[Y], mAlpha.y }, mGizmoRadius, GizmoMode::SCALE);
			drawGizmoLine(Z, pos, { mColors[Z], mAlpha.z }, mGizmoRadius, GizmoMode::SCALE);

			const Math::Mat4 scaleMat = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{mScaleCoef});
			const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
			Utils::renderCircle(pos, ECSHandler::registry().getComponent<TransformComponent>(camera)->getQuaternion(), scaleMat, mGizmoRadius + mCommonGizmoOffset, {1.f, 1.f, 1.f, mAlphaCommonGizmo }, 64);
		}
		else {
			Utils::renderLine(mStartPos, mIntersectionCurrentPos, { 0.f,0.f,0.f,mHintAlpha }, 1.f);

			drawVec3Text(scale);

			drawAxis(mStartPos, mActiveAxis);
			if (isLineAxis(mActiveAxis)) {
				drawGizmoLine(mActiveAxis, mStartPos, mInactiveColor, mGizmoRadius, GizmoMode::SCALE);
	
				const auto delta = Math::distance(mIntersectionCurrentPos, mStartPos) / Math::distance(mIntersectionStartPos, mStartPos);
				drawGizmoLine(mActiveAxis, pos, { mColors[mActiveAxis],1.f }, mGizmoRadius * delta, GizmoMode::SCALE);
			}
		}
	}

	void Gizmo::drawMoveGizmo(const Math::Vec3& pos) {
		drawPos(pos, { 0.f,0.f,0.f, mHintAlpha });

		drawPlaneQuads(pos);

		if (mActiveAxis == Axis::NONE) {
			drawGizmoLine(X, pos, { mColors[X], mAlpha.x }, mGizmoRadius, GizmoMode::MOVE);
			drawGizmoLine(Y, pos, { mColors[Y], mAlpha.y }, mGizmoRadius, GizmoMode::MOVE);
			drawGizmoLine(Z, pos, { mColors[Z], mAlpha.z }, mGizmoRadius, GizmoMode::MOVE);
		}
		else {
			Utils::renderLine(mStartPos, pos, { 0.f,0.f,0.f,mHintAlpha }, 1.f);

			drawVec3Text(pos - mStartPos);

			drawPos(mStartPos, mInactiveColor);

			drawAxis(mStartPos, mActiveAxis);
			if (isLineAxis(mActiveAxis)) {
				drawGizmoLine(mActiveAxis, mStartPos, mInactiveColor, mGizmoRadius, GizmoMode::MOVE);
				drawGizmoLine(mActiveAxis, pos, { mColors[mActiveAxis], mAlpha[mActiveAxis - 1]}, mGizmoRadius, GizmoMode::MOVE);
			}
		}
	}

	void Gizmo::updateGizmosAlpha(const Math::Vec3& cameraPos, const Math::Vec3& entityPos) {
		Math::Vec3 dot;

		auto cameraToEntityDir = normalize(entityPos - cameraPos);

		dot.x = 1.f - std::fabs(Math::dot(cameraToEntityDir, mAxisDirection[X]));
		dot.x = dot.x > mAxisVisibilityKoef ? 1.f : dot.x / mAxisVisibilityKoef;

		dot.y = 1.f - std::fabs(Math::dot(cameraToEntityDir, mAxisDirection[Y]));
		dot.y = dot.y > mAxisVisibilityKoef ? 1.f : dot.y / mAxisVisibilityKoef;

		dot.z = 1.f - std::fabs(Math::dot(cameraToEntityDir, mAxisDirection[Z]));
		dot.z = dot.z > mAxisVisibilityKoef ? 1.f : dot.z / mAxisVisibilityKoef;

		Math::Vec3 planesDot;
		auto entityToCameraPlaneDir = cameraPos;
		entityToCameraPlaneDir.x = entityPos.x;
		planesDot.x = 1.f - std::fabs(Math::dot(cameraToEntityDir, normalize(entityToCameraPlaneDir - entityPos)));
		planesDot.x = planesDot.x > mPlaneVisibilityKoef ? 1.f : planesDot.x / mPlaneVisibilityKoef;

		entityToCameraPlaneDir = cameraPos;
		entityToCameraPlaneDir.y = entityPos.y;
		planesDot.y = 1.f - std::fabs(Math::dot(cameraToEntityDir, normalize(entityToCameraPlaneDir - entityPos)));
		planesDot.y = planesDot.y > mPlaneVisibilityKoef ? 1.f : planesDot.y / mPlaneVisibilityKoef;

		entityToCameraPlaneDir = cameraPos;
		entityToCameraPlaneDir.z = entityPos.z;
		planesDot.z = 1.f - std::fabs(Math::dot(cameraToEntityDir, normalize(entityToCameraPlaneDir - entityPos)));
		planesDot.z = planesDot.z > mPlaneVisibilityKoef ? 1.f : planesDot.z / mPlaneVisibilityKoef;


		mAlpha.x = mHoveredAxis == Axis::X ? mHoveredAlpha : mNotHoveredAlpha;
		mAlpha.y = mHoveredAxis == Axis::Y ? mHoveredAlpha : mNotHoveredAlpha;
		mAlpha.z = mHoveredAxis == Axis::Z ? mHoveredAlpha : mNotHoveredAlpha;

		mAlphaPlanes.x = mHoveredAxis == Axis::YZ ? mHoveredAlpha : mNotHoveredAlpha;
		mAlphaPlanes.y = mHoveredAxis == Axis::XZ ? mHoveredAlpha : mNotHoveredAlpha;
		mAlphaPlanes.z = mHoveredAxis == Axis::XY ? mHoveredAlpha : mNotHoveredAlpha;

		mAlphaPlanes *= planesDot;

		if (mCurrentMode == GizmoMode::ROTATE) {
			mAlpha *= planesDot;
		}
		else {
			mAlpha *= dot;
		}

		mAlphaCommonGizmo = mHoveredAxis == Axis::XYZ ? mHoveredAlpha : mNotHoveredAlpha;
	}

	//utils
	Math::Vec3 Gizmo::findIntersectionRayWithPlane(const Math::Ray& ray, const PhysicsEngine::Triangle& plane) {
		bool parallel = false;
		const auto res = PhysicsEngine::Physics::findIntersectionLinePlane(ray.a, ray.a + ray.direction, plane, parallel);
		if (parallel) {
			return {};
		}
		return res;
	}

	PhysicsEngine::Triangle Gizmo::getMouseRayPlane(const Math::Ray& ray, Axis plane, TransformComponent* cameraTransform, const Math::Vec3& initialPos) {
		switch (plane) {
		case X: return getMouseRayPlane(ray, YZ, cameraTransform, initialPos);
		case Y: return getMouseRayPlane(ray, XZ, cameraTransform, initialPos);
		case Z: return getMouseRayPlane(ray, XY, cameraTransform, initialPos);
		case XY: return { initialPos, initialPos + ray.direction, initialPos + cameraTransform->getUp() };
		case XZ: return { initialPos, initialPos + ray.direction, initialPos + cameraTransform->getRight() };
		case YZ: return { initialPos, initialPos + ray.direction, initialPos + cameraTransform->getUp() };
		default: return {};
		}
	}

	PhysicsEngine::Triangle Gizmo::getCamPlane(Axis plane, TransformComponent* cameraTransform, const Math::Vec3& initialPos) {
		switch (plane) {
		case X: return getCamPlane(YZ, cameraTransform, initialPos);
		case Y: return getCamPlane(XZ, cameraTransform, initialPos);
		case Z: return getCamPlane(XY, cameraTransform, initialPos);
		case XY: return { initialPos, initialPos + cameraTransform->getRight(), initialPos + cameraTransform->getUp() };
		case XZ: return { initialPos, initialPos + cameraTransform->getForward(), initialPos + cameraTransform->getRight() };
		case YZ: return { initialPos, initialPos + cameraTransform->getForward(), initialPos + cameraTransform->getUp() };
		case XYZ: return getCamPlane(XY, cameraTransform, initialPos);
		default: return {};
		}
	}

	//draw utils
	void Gizmo::drawPlaneQuads(const Math::Vec3& pos) {
		static const std::array PLANE_ROTATIONS {
			Math::Quaternion{ 1.f, 0.f, 0.f, 0.f},
			Math::Quaternion{ 0.707f, 0.f, -0.707f, 0.f, },
			Math::Quaternion{ 0.707f, 0.707f, 0.f, 0.f },
			Math::Quaternion{ 1.f, 0.f, 0.f, 0.f },
		};

		auto drawQuad = [this](const Math::Vec3& pos, Axis axis, float start, float end, const Math::Vec4& color) {
			Utils::renderQuad(Math::Vec3{start, start, 0.f}, Math::Vec3{end, end, 0.f}, PLANE_ROTATIONS[axis].toMat4(), pos, color);
		};

		const float start = mPlaneQaudOffset * mScaleCoef;
		const float end = (mPlaneQaudOffset + mPlaneQaudSize) * mScaleCoef;

		if (mActiveAxis == Axis::NONE) {
			drawQuad(pos, X, start, end, { mColors[X], mAlphaPlanes[X - 1] });
			drawQuad(pos, Y, start, end, { mColors[Y], mAlphaPlanes[Y - 1] });
			drawQuad(pos, Z, start, end, { mColors[Z], mAlphaPlanes[Z - 1] });
		}
		else {
			if (mActiveAxis == XY) {
				drawQuad(mStartPos, Z, start, end, mInactiveColor);
				drawQuad(pos, Z, start, end, { mColors[Z], mAlphaPlanes[X - 1] });
			}
			else if (mActiveAxis == YZ) {
				drawQuad(mStartPos, X, start, end, mInactiveColor);
				drawQuad(pos, X, start, end, { mColors[X], mAlphaPlanes[X - 1] });
			}
			else if (mActiveAxis == XZ) {
				drawQuad(mStartPos, Y, start, end, mInactiveColor);
				drawQuad(pos, Y, start, end, { mColors[Y], mAlphaPlanes[X - 1] });
			}
		}
	}

	void Gizmo::drawGizmoLine(Axis axis, const Math::Vec3& pos, const Math::Vec4& color, float end, GizmoMode type) const {
		if (!isLineAxis(axis)) {
			return;
		}

		Utils::renderLine(pos + mAxisDirection[axis] * mGizmoStartRadius * mScaleCoef, pos + mAxisDirection[axis] * end * mScaleCoef, color, 3.f);
		if (type == GizmoMode::SCALE) {
			drawScaleCube(axis, pos, color, end);
		}
		else if (type == GizmoMode::MOVE) {
			static const std::array MOVE_ROTATIONS {
				Math::Quaternion{1.f, 0.f, 0.f, 0.f},
				Math::Quaternion{ 0.707f, 0.f, 0.f, -0.707f },
				Math::Quaternion{ 1.f, 0.f, 0.f, 0.f },
				Math::Quaternion{  0.707f, 0.707f, 0.f, 0.f  },
			};

			const auto scaleMat = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{mScaleCoef});

			Utils::renderCone(pos + mAxisDirection[axis] * end * mScaleCoef, MOVE_ROTATIONS[axis], scaleMat, mTipRadius, mTipHeight, color, 32);
		}
	}

	void Gizmo::drawScaleCube(Axis axis, const Math::Vec3& pos, const Math::Vec4& color, float start) const {
		constexpr  Math::Quaternion<float> quat;
		
		auto cubeStart = mAxisDirection[axis] * start;
		cubeStart[axis - 1] += mTipRadius;
		cubeStart -= mTipRadius;

		auto cubeEnd = mAxisDirection[axis] * start;
		cubeEnd[axis - 1] += mTipHeight - mTipRadius;
		cubeEnd += mTipRadius;

		Utils::renderCubeMesh(cubeStart * mScaleCoef, cubeEnd * mScaleCoef, quat.toMat4(), pos, color);
	}

	void Gizmo::drawAxis(const Math::Vec3& pos, Axis axis) const {
		if (axis > Axis::Z) {
			if (axis == XY) {
				drawAxis(pos, X);
				drawAxis(pos, Y);
			}
			else if (axis == XZ) {
				drawAxis(pos, X);
				drawAxis(pos, Z);
			}
			else if (axis == YZ) {
				drawAxis(pos, Y);
				drawAxis(pos, Z);
			}
		}
		else {
			Utils::renderLine(pos - mAxisDirection[axis] * Renderer::drawDistance, pos + mAxisDirection[axis] * Renderer::drawDistance, { mColors[axis], mHintAlpha }, 2.5f);
		}
	}

	void Gizmo::drawPos(const Math::Vec3& pos, const Math::Vec4& color, float size) const {
		size *= mScaleCoef;
		Utils::renderLine(pos - mAxisDirection[X] * size, pos + mAxisDirection[X] * size, color, 1.f);
		Utils::renderLine(pos - mAxisDirection[Y] * size, pos + mAxisDirection[Y] * size, color, 1.f);
		Utils::renderLine(pos - mAxisDirection[Z] * size, pos + mAxisDirection[Z] * size, color, 1.f);
	}

	void Gizmo::drawVec3Text(const Math::Vec3& value) const {
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

		const auto size = ImGui::CalcTextSize(std::string(std::to_string(value.x) + std::to_string(value.y) + std::to_string(value.z) + "___").c_str());

		ImGui::SetNextWindowPos(ImVec2(mMousePos.x + ImGui::GetMainViewport()->Pos.x, mMousePos.y + ImGui::GetMainViewport()->Pos.y - size.y * 2.f));
		ImGui::SetNextWindowSize(size);
		ImGui::Begin("angle", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		ImGui::Text("%f:%f:%f", value.x, value.y, value.z);
		ImGui::End();
	}

	void Gizmo::drawFloatText(float value) const {
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

		const auto size = ImGui::CalcTextSize(std::string(std::to_string(value) + "  deg").c_str());

		ImGui::SetNextWindowPos(ImVec2(mMousePos.x + ImGui::GetMainViewport()->Pos.x, mMousePos.y + ImGui::GetMainViewport()->Pos.y - size.y * 2.f));
		ImGui::SetNextWindowSize(size);
		ImGui::Begin("angle", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
		ImGui::Text("%f deg", value);
		ImGui::End();
	}
}
