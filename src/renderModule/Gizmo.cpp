#include "Gizmo.h"

#include "imgui.h"
#include "Utils.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "mathModule/CameraUtils.h"
#include "myPhysicsEngine/Physics.h"
#include "systemsModule/systems/CameraSystem.h"

namespace SFE::RenderModule {
	Gizmo::Gizmo() {
		auto cone = AssetsModule::ModelLoader::instance()->load("models/cone.fbx");
		if (!cone->lods.empty()) {
			if (!cone->lods[0].mMeshHandles.empty()) {
				for (auto i = 0; i < cone->lods[0].mMeshHandles.front().mData->mIndices.size(); i += 3) {
					Utils::Triangle tr;
					tr.A = cone->lods[0].mMeshHandles.front().mData->mVertices[cone->lods[0].mMeshHandles.front().mData->mIndices[i]].mPosition;
					tr.B = cone->lods[0].mMeshHandles.front().mData->mVertices[cone->lods[0].mMeshHandles.front().mData->mIndices[i + 1]].mPosition;
					tr.C = cone->lods[0].mMeshHandles.front().mData->mVertices[cone->lods[0].mMeshHandles.front().mData->mIndices[i + 2]].mPosition;

					mCone.push_back(tr);
				}
			}
		}

		static Math::Vec3 offset;
		onMouseBtnEvent = [this](Math::DVec2 mousePos, CoreModule::MouseButton btn, CoreModule::InputEventType eventType) {
			if (mCurrentMode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity) || mHoveredAxis == NONE) {
				return;
			}

			if (btn == CoreModule::MouseButton::MOUSE_BUTTON_LEFT && eventType == CoreModule::InputEventType::PRESS) {
				mMousePos = Math::Vec2(mousePos);

				mActiveAxis = mHoveredAxis;

				const auto hovered = getHoveredGizmo(mCurrentMode);
				if (mCurrentMode == GizmoMode::ROTATE) {
					mPrevPos = hovered.second;
					mAnglePos = mPrevPos;
				}
				else {
					const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);

					const auto entityPos = transformComp->getPos(true);
					offset = hovered.second - entityPos;
					mPrevPosPos = hovered.second;
					if (mActiveAxis) {
						mPrevPos = entityPos;
						mPrevScale = transformComp->getScale();
					}
				}
			}
			else {
				mMousePos = {};
				mActiveAxis = Axis::NONE;
			}
		};

		onMouseEvent = [this](Math::DVec2 mousePos, Math::DVec2 mouseOffset) {
			if (mCurrentMode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity)) {
				return;
			}

			mMousePos = { mousePos };
			if (mActiveAxis == Axis::NONE) {
				mHoveredAxis = getHoveredGizmo(mCurrentMode).first;
				return;
			}

			const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
			auto camRay = Math::calcCameraRay(camera, mMousePos);

			auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
			auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
			auto entityPos = transformComp->getPos(true);

			if (mCurrentMode == GizmoMode::MOVE) {
				Math::Vec3 intersection = findIntersecRayWithPlane({ entityPos, mAxisDirection[mActiveAxis] }, getCamPlane(camRay, mAxisPlane[mActiveAxis], cameraTransform));
				
				transformComp->setPos(entityPos + (intersection - mPrevPosPos));
				mPrevPosPos = intersection;
			}
			else if (mCurrentMode == GizmoMode::SCALE) {
				Math::Vec3 intersection = findIntersecRayWithPlane({ entityPos, mAxisDirection[mActiveAxis] }, getCamPlane(camRay, mAxisPlane[mActiveAxis], cameraTransform));
		
				auto scaleDir = transformComp->getQuaternion().rotateVector(mAxisDirection[mActiveAxis]);
				scaleDir = { std::fabs(scaleDir.x), std::fabs(scaleDir.y), std::fabs(scaleDir.z) };
				const auto distance = intersection - mPrevPos;
				const auto koef = mPrevScale / offset[mActiveAxis - 1];

				transformComp->setScale(mPrevScale + scaleDir * (distance[mActiveAxis - 1] - offset[mActiveAxis - 1]) * koef);
			}
			else if (mCurrentMode == GizmoMode::ROTATE) {
				if (mActiveAxis == X || mActiveAxis == Y || mActiveAxis == Z) {
					const auto oldPos = entityPos + Math::normalize(mAnglePos - entityPos) * 150.f;
					mAnglePos = findIntersecRayWithPlane(camRay, mPlanes[mActiveAxis] + entityPos);
					const auto newPos = entityPos + Math::normalize(mAnglePos - entityPos) * 150.f;

					auto angleDif = Math::calcAngleOnCircle(Math::distance(oldPos, newPos), 150.f);
					if (Math::cross(oldPos - entityPos, newPos - entityPos) < Math::Vec3(0.f)) {
						angleDif = 2.0f * Math::pi<float>() - angleDif;
					}
					angleDif = Math::degrees(angleDif);

					Math::Quaternion<float> q;
					q.eulerToQuaternion(mAxisDirection[mActiveAxis] * angleDif);
					q = q * transformComp->getQuaternion();
					transformComp->setRotate(Math::degrees(q.toEuler()));
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
		if (!ECSHandler::registry().isEntity(mEntity)) {
			return;
		}

		if (mCurrentMode == GizmoMode::NONE) {
			return;
		}

		auto transform = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		auto& pos = transform->getPos(true);
		auto& scale = transform->getScale();
		Math::Quaternion<float> quat;

		if (mActiveAxis == Axis::NONE) {
			mAlpha.x = mHoveredAxis == Axis::X ? 0.8f : 0.6f;
			mAlpha.y = mHoveredAxis == Axis::Y ? 0.8f : 0.6f;
			mAlpha.z = mHoveredAxis == Axis::Z ? 0.8f : 0.6f;

			if (mCurrentMode == GizmoMode::MOVE || mCurrentMode == GizmoMode::SCALE) {
				Utils::renderLine(pos + Math::Vec3(20.f, 0.f, 0.f), pos + Math::Vec3(200.f, 0.f, 0.f), { 1.f,0.f,0.f,mAlpha.x }, 5.f);
				Utils::renderLine(pos + Math::Vec3(0.f, 20.f, 0.f), pos + Math::Vec3(0.f, 200.f, 0.f), { 0.f,1.f,0.f,mAlpha.y }, 5.f);
				Utils::renderLine(pos + Math::Vec3(0.f, 0.f, 20.f), pos + Math::Vec3(0.f, 0.f, 200.f), { 0.f,0.f,1.f,mAlpha.z }, 5.f);

				Utils::renderQuad(Math::Vec3{15.f, 15.f, 0.f}, Math::Vec3{30.f, 30.f, 0.f}, quat.toMat4(), pos, { 0.f,0.f,1.f,mAlpha.z });
				Utils::renderQuad(Math::Vec3{15.f, 0.f, 15.f}, Math::Vec3{30.f, 0.f, 30.f}, quat.toMat4(), pos, { 0.f,1.f,0.f,mAlpha.y });
				Utils::renderQuad(Math::Vec3{0.f, 15.f, 15.f}, Math::Vec3{0.f, 30.f, 30.f}, quat.toMat4(), pos, { 1.f,0.f,0.f,mAlpha.x });
			}

			Utils::renderLine(pos - Math::Vec3(10.f, 0.f, 0.f), pos + Math::Vec3(10.f, 0.f, 0.f), { 0.f,0.f,0.f,0.6f }, 2.f);
			Utils::renderLine(pos - Math::Vec3(0.f, 10.f, 0.f), pos + Math::Vec3(0.f, 10.f, 0.f), { 0.f,0.f,0.f,0.6f }, 2.f);
			Utils::renderLine(pos - Math::Vec3(0.f, 0.f, 10.f), pos + Math::Vec3(0.f, 0.f, 10.f), { 0.f,0.f,0.f,0.6f }, 2.f);
			if (mCurrentMode == GizmoMode::MOVE) {
				auto scaleMat = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{0.08f, 0.15f, 0.08f, });
				auto matrix0 = Math::translate(Math::Mat4{1.f}, pos + Math::Vec3{0.f, 200.f, 0.f}) * quat.toMat4() * scaleMat;
				quat = { 0.707f, 0.f,0.f,-0.707f };
				auto matrix1 = Math::translate(Math::Mat4{1.f}, pos + Math::Vec3{200.f, 0.f, 0.f}) * quat.toMat4() * scaleMat;
				quat = { 0.707f, 0.707f,0.f,0.f };
				auto matrix2 = Math::translate(Math::Mat4{1.f}, pos + Math::Vec3{0.f, 0.f, 200.f}) * quat.toMat4() * scaleMat;

				for (auto triangle : mCone) {
					Utils::Triangle triangleY {
					matrix0 * Math::Vec4(triangle.A, 1.f),
					matrix0 * Math::Vec4(triangle.B, 1.f),
					matrix0 * Math::Vec4(triangle.C, 1.f)
					};

					Utils::renderTriangle(triangleY, { 0.f,1.f,0.f,mAlpha.y });

					Utils::Triangle triangleX {
					matrix1 * Math::Vec4(triangle.A, 1.f),
					matrix1 * Math::Vec4(triangle.B, 1.f),
					matrix1 * Math::Vec4(triangle.C, 1.f)
					};

					Utils::renderTriangle(triangleX, { 1.f,0.f,0.f,mAlpha.x });

					Utils::Triangle triangleZ  {
						matrix2 * Math::Vec4(triangle.A, 1.f),
						matrix2 * Math::Vec4(triangle.B, 1.f),
						matrix2 * Math::Vec4(triangle.C, 1.f)
					};

					Utils::renderTriangle(triangleZ, { 0.f,0.f,1.f,mAlpha.z });
				}
			}
			else if (mCurrentMode == GizmoMode::SCALE) {
				drawScaleGizmo(pos);
				Utils::renderCubeMesh(Math::Vec3(195.f, -3.f, -3.f), Math::Vec3(205.f, 3.f, 3.f), quat.toMat4(), pos, { 1.f,0.f,0.f,mAlpha.x });
				Utils::renderCubeMesh(Math::Vec3(-3.f, 195.f, -3.f), Math::Vec3(3.f, 205.f, 3.f), quat.toMat4(), pos, { 0.f,1.f,0.f,mAlpha.y });
				Utils::renderCubeMesh(Math::Vec3(-3.f, -3.f, 195.f), Math::Vec3(3.f, 3.f, 205.f), quat.toMat4(), pos, { 0.f,0.f,1.f,mAlpha.z });
			}
			else if (mCurrentMode == GizmoMode::ROTATE) {
				drawRotateGizmo(pos);
			}
		}
		else {
			if (mCurrentMode == GizmoMode::ROTATE) {
				drawRotateGizmo(pos);
			}
			else {
				//show prev pos
				Utils::renderLine(mPrevPos - Math::Vec3(10.f, 0.f, 0.f), mPrevPos + Math::Vec3(10.f, 0.f, 0.f), { 0.f,0.f,0.f,0.6f }, 2.f);
				Utils::renderLine(mPrevPos - Math::Vec3(0.f, 10.f, 0.f), mPrevPos + Math::Vec3(0.f, 10.f, 0.f), { 0.f,0.f,0.f,0.6f }, 2.f);
				Utils::renderLine(mPrevPos - Math::Vec3(0.f, 0.f, 10.f), mPrevPos + Math::Vec3(0.f, 0.f, 10.f), { 0.f,0.f,0.f,0.6f }, 2.f);
			}
		}

		if (mCurrentMode == GizmoMode::ROTATE) {

		}
		else {
			switch (mActiveAxis) {
			case Axis::Y: {
				auto scaleKoef = 1.f + (scale.y - mPrevScale.y) / mPrevScale.y;
				Utils::renderLine(pos + Math::Vec3(0.f, -Renderer::drawDistance, 0.f), pos + Math::Vec3(0.f, Renderer::drawDistance, 0.f), { 0.f,1.f,0.f,0.5f }, 2.5f);
				Utils::renderLine(mPrevPos + Math::Vec3(0.f, 20.f, 0.f), mPrevPos + Math::Vec3(0.f, 200.f, 0.f), { 1.f,1.f,1.f,0.6f }, 5.f);
				Utils::renderLine(pos + Math::Vec3(0.f, 20.f, 0.f), pos + Math::Vec3(0.f, 200.f * scaleKoef, 0.f), { 0.f,1.f,0.f,1.f }, 5.f);
				if (mCurrentMode == GizmoMode::MOVE) {
					auto scaleMat = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{0.08f, 0.15f, 0.08f, });
					auto matrix2 = Math::translate(Math::Mat4{1.f}, pos + Math::Vec3{0.f, 200.f, 0.f})* quat.toMat4()* scaleMat;
					auto matrixOld = Math::translate(Math::Mat4{1.f}, mPrevPos + Math::Vec3{0.f, 200.f, 0.f})* quat.toMat4()* scaleMat;

					for (auto triangle : mCone) {

						Utils::renderTriangle({
							matrixOld * Math::Vec4(triangle.A, 1.f),
							matrixOld * Math::Vec4(triangle.B, 1.f),
							matrixOld * Math::Vec4(triangle.C, 1.f)
							}, { 1.f,1.f,1.f,1.0f });

						Utils::renderTriangle({
							matrix2 * Math::Vec4(triangle.A, 1.f),
							matrix2 * Math::Vec4(triangle.B, 1.f),
							matrix2 * Math::Vec4(triangle.C, 1.f)
							}, { 0.f,1.f,0.f,1.0f });
					}
				}
				else if (mCurrentMode == GizmoMode::SCALE) {
					Utils::renderCubeMesh(Math::Vec3(-3.f, 195.f, -3.f), Math::Vec3(3.f, 205.f, 3.f), quat.toMat4(), mPrevPos, { 1.f,1.f,1.f,0.6f });
					Utils::renderCubeMesh(Math::Vec3(-3.f, 195.f * scaleKoef, -3.f), Math::Vec3(3.f, 195.f * scaleKoef + 10.f, 3.f), quat.toMat4(), pos, { 0.f,1.f,0.f,1.f });
				}
				break;
			}
			case Axis::X: {
				auto scaleKoef = 1.f + (scale.x - mPrevScale.x) / mPrevScale.x;
				Utils::renderLine(pos + Math::Vec3(-Renderer::drawDistance, 0.f, 0.f), pos + Math::Vec3(Renderer::drawDistance, 0.f, 0.f), { 1.f,0.f,0.f,0.5f }, 2.5f);
				Utils::renderLine(mPrevPos + Math::Vec3(20.f, 0.f, 0.f), mPrevPos + Math::Vec3(200.f, 0.f, 0.f), { 1.f,1.f,1.f,0.6f }, 5.f);
				Utils::renderLine(pos + Math::Vec3(20.f, 0.f, 0.f), pos + Math::Vec3(200.f * scaleKoef, 0.f, 0.f), { 1.f,0.f,0.f,1.f }, 5.f);
				if (mCurrentMode == GizmoMode::MOVE) {
					auto scaleMat = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{0.08f, 0.15f, 0.08f, });
					quat = { 0.707f, 0.f,0.f,-0.707f };
					auto matrix2 = Math::translate(Math::Mat4{1.f}, pos + Math::Vec3{200.f, 0.f, 0.f})* quat.toMat4()* scaleMat;
					auto matrixOld = Math::translate(Math::Mat4{1.f}, mPrevPos + Math::Vec3{200.f, 0.f, 0.f})* quat.toMat4()* scaleMat;

					for (auto triangle : mCone) {

						Utils::renderTriangle({
							matrixOld * Math::Vec4(triangle.A, 1.f),
							matrixOld * Math::Vec4(triangle.B, 1.f),
							matrixOld * Math::Vec4(triangle.C, 1.f)
							}, { 1.f,1.f,1.f,1.0f });

						Utils::renderTriangle({
							matrix2 * Math::Vec4(triangle.A, 1.f),
							matrix2 * Math::Vec4(triangle.B, 1.f),
							matrix2 * Math::Vec4(triangle.C, 1.f)
							}, { 1.f,0.f,0.f,1.0f });
					}
				}
				else if (mCurrentMode == GizmoMode::SCALE) {

					Utils::renderCubeMesh(Math::Vec3(195.f * scaleKoef, -3.f, -3.f), Math::Vec3(195.f * scaleKoef + 10.f, 3.f, 3.f), quat.toMat4(), pos, { 1.f,0.f,0.f,1.0f });
					Utils::renderCubeMesh(Math::Vec3(195.f, -3.f, -3.f), Math::Vec3(205.f, 3.f, 3.f), quat.toMat4(), mPrevPos, { 1.f,1.f,1.f,0.6f });
				}
				break;
			}
			case Axis::Z: {
				auto scaleKoef = 1.f + (scale.z - mPrevScale.z) / mPrevScale.z;
				Utils::renderLine(pos + Math::Vec3(0.f, 0.f, -Renderer::drawDistance), pos + Math::Vec3(0.f, 0.f, Renderer::drawDistance), { 0.f,0.f,1.f,0.5f }, 2.5f);
				Utils::renderLine(mPrevPos + Math::Vec3(0.f, 0.f, 20.f), mPrevPos + Math::Vec3(0.f, 0.f, 200.f), { 1.f,1.f,1.f,0.6f }, 5.f);
				Utils::renderLine(pos + Math::Vec3(0.f, 0.f, 20.f), pos + Math::Vec3(0.f, 0.f, 200.f * scaleKoef), { 0.f,0.f,1.f,1.f }, 5.f);

				if (mCurrentMode == GizmoMode::MOVE) {
					auto scaleMat = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{0.08f, 0.15f, 0.08f, });
					quat = { 0.707f, 0.707f,0.f,0.f };
					auto matrix2 = Math::translate(Math::Mat4{1.f}, pos + Math::Vec3{0.f, 0.f, 200.f})* quat.toMat4()* scaleMat;
					auto matrixOld = Math::translate(Math::Mat4{1.f}, mPrevPos + Math::Vec3{0.f, 0.f, 200.f})* quat.toMat4()* scaleMat;

					for (auto triangle : mCone) {

						Utils::renderTriangle({
							matrixOld * Math::Vec4(triangle.A, 1.f),
							matrixOld * Math::Vec4(triangle.B, 1.f),
							matrixOld * Math::Vec4(triangle.C, 1.f)
							}, { 1.f,1.f,1.f,1.0f });

						Utils::renderTriangle({
							matrix2 * Math::Vec4(triangle.A, 1.f),
							matrix2 * Math::Vec4(triangle.B, 1.f),
							matrix2 * Math::Vec4(triangle.C, 1.f)
							}, { 0.f,0.f,1.f,1.0f });
					}
				}
				else if (mCurrentMode == GizmoMode::SCALE) {
					Utils::renderCubeMesh(Math::Vec3(-3.f, -3.f, 195.f * scaleKoef), Math::Vec3(3.f, 3.f, 195.f * scaleKoef + 10.f), quat.toMat4(), pos, { 0.f,0.f,1.f,1.0f });
					Utils::renderCubeMesh(Math::Vec3(-3.f, -3.f, 195.f), Math::Vec3(3.f, 3.f, 205.f), quat.toMat4(), mPrevPos, { 1.f,1.f,1.f,0.6f });
				}

				break;
			}
			default:;
			}
		}
	}

	Math::Vec3 Gizmo::findIntersecRayWithPlane(const Math::Ray& ray, const PhysicsEngine::Triangle& plane) {
		bool parallel = false;
		auto res = PhysicsEngine::Physics::findIntersectionLinePlane(ray.a, ray.a + ray.direction, plane, parallel);
		if (parallel) {
			return {};
		}
		return res;
	}

	PhysicsEngine::Triangle Gizmo::getCamPlane(const Math::Ray& ray, Axis plane, TransformComponent* cameraTransform) {
		switch (plane) {
		case XY: return { ray.a, ray.a + ray.direction, ray.a + cameraTransform->getUp() };
		case XZ: return { ray.a, ray.a + ray.direction, ray.a + cameraTransform->getRight() };
		case YZ: return { ray.a, ray.a + ray.direction, ray.a + cameraTransform->getUp() };
		default: return {};
		}
	}

	std::pair<Axis, Math::Vec3> Gizmo::getHoveredGizmo(GizmoMode mode) {
		if (mode == GizmoMode::NONE || !ECSHandler::registry().isEntity(mEntity)) {
			return {};
		}

		const auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
		const auto camRay = Math::calcCameraRay(camera, mMousePos);

		const auto transformComp = ECSHandler::registry().getComponent<TransformComponent>(mEntity);
		const auto entityPos = transformComp->getPos(true);

		if (mode == GizmoMode::ROTATE) {
			auto posZ = findIntersecRayWithPlane(camRay, mPlanes[Z] + entityPos);
			auto distZ = Math::distance(entityPos, posZ);
			if (distZ < 160.f && distZ > 140.f) {
				return { Axis::Z , posZ };
			}

			auto posY = findIntersecRayWithPlane(camRay, mPlanes[Y] + entityPos);
			auto distY = Math::distance(entityPos, posY);
			if (distY < 160.f && distY > 140.f) {
				return { Axis::Y , posY };
			}

			auto posX = findIntersecRayWithPlane(camRay, mPlanes[X] + entityPos);
			auto distX = Math::distance(entityPos, posX);
			if (distX < 160.f && distX > 140.f) {
				return { Axis::X , posX };
			}
		}
		else {
			auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);

			auto resX = findIntersecRayWithPlane({ entityPos ,mAxisDirection[X] }, getCamPlane(camRay, mAxisPlane[X], cameraTransform));
			auto resY = findIntersecRayWithPlane({ entityPos ,mAxisDirection[Y] }, getCamPlane(camRay, mAxisPlane[Y], cameraTransform));
			auto resZ = findIntersecRayWithPlane({ entityPos ,mAxisDirection[Z] }, getCamPlane(camRay, mAxisPlane[Z], cameraTransform));

			auto YD = PhysicsEngine::Physics::distancePointToLine(resY, camRay.a, camRay.direction);
			auto XD = PhysicsEngine::Physics::distancePointToLine(resX, camRay.a, camRay.direction);
			auto ZD = PhysicsEngine::Physics::distancePointToLine(resZ, camRay.a, camRay.direction);

			if (XD < YD && XD < ZD && XD < 20.f && resX.x - entityPos.x > 20.f && resX.x - entityPos.x < 200.f) {
				return { Axis::X, resX };
			}

			if (YD < XD && YD < ZD && YD < 20.f && resY.y - entityPos.y > 20.f && resY.y - entityPos.y < 200.f) {
				return { Axis::Y, resY };
			}
			
			if (ZD < YD && ZD < XD && ZD < 20.f && resZ.z - entityPos.z > 20.f && resZ.z - entityPos.z < 200.f) {
				return { Axis::Z, resZ };
			}
		}
		return{};
	}

	void Gizmo::drawRotateGizmo(const Math::Vec3& pos) {
		const Math::Mat4 scale = Math::scale(Math::Mat4{ 1.f }, Math::Vec3{1.f});
		constexpr std::array rotations {
			Math::Quaternion{1.f,0.f,0.f,0.f},
			Math::Quaternion{ 0.707f, 0.f, -0.707f, 0.f },
			Math::Quaternion{ 0.707f, 0.707f, 0.f, 0.f },
			Math::Quaternion{ 0.f, 1.f, 0.f, 0.f },
		};

		if (mActiveAxis == NONE) {
			Utils::renderCircle(pos, rotations[X], scale, 150.f, { mColors[Axis::X], mAlpha.x }, 64);
			Utils::renderCircle(pos, rotations[Y], scale, 150.f, { mColors[Axis::Y], mAlpha.y }, 64);
			Utils::renderCircle(pos, rotations[Z], scale, 150.f, { mColors[Axis::Z], mAlpha.z }, 64);
		}
		else if (mActiveAxis == X || mActiveAxis == Y || mActiveAxis == Z) {
			const auto dd = mAxisDirection[mActiveAxis] * Renderer::drawDistance;
			//axis line
			Utils::renderLine(pos - dd, pos + dd, { mColors[mActiveAxis],0.3f }, 1.5f);

			Utils::renderCircle(pos, rotations[mActiveAxis], scale, 150.f, {mColors[mActiveAxis],1.f}, 64);

			Axis axis = mActiveAxis == X ? Z : mActiveAxis == Y ? X : mActiveAxis == Z ? X : NONE;

			auto prevAngle = Math::degrees(Math::calcAngleBetweenVectors(mPrevPos - pos, mAxisDirection[axis]));

			auto oldPos = pos + Math::normalize(mPrevPos - pos) * 150.f;
			auto newPos = pos + Math::normalize(mAnglePos - pos) * 150.f;

			auto difAngle = Math::calcAngleOnCircle(Math::distance(oldPos, newPos), 150.f);
			ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
			auto posW = ImGui::GetMainViewport()->Pos;

			
			if (Math::cross(oldPos - pos, newPos - pos) < Math::Vec3(0.f)) {
				difAngle = 2.0f * Math::pi<float>() - difAngle;
			}

			difAngle = Math::degrees(difAngle);
			auto size = ImGui::CalcTextSize(std::string(std::to_string(difAngle) + "  deg").c_str());
			auto mPos = ImVec2(mMousePos.x + posW.x, mMousePos.y + posW.y - size.y * 2.f);
			ImGui::SetNextWindowPos(mPos);
			ImGui::SetNextWindowSize(size);
			ImGui::Begin("angle", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav);
			ImGui::Text("%f deg", difAngle);
			ImGui::End();

			Utils::renderCircleFilled(pos, rotations[mActiveAxis], scale, 150.f, { mColors[mActiveAxis], 0.1f }, 32, prevAngle, /*mAngleGrow ? 360.f - difAngle :*/ -difAngle);
			Utils::renderCubeMesh(Math::Vec3(-3.f, -3.f, -3.f), Math::Vec3(3.f, 3.f, 3.f), rotations[0].toMat4(), newPos, { mColors[mActiveAxis], 0.3f});
			//segment lines
			Utils::renderLine(pos, oldPos, { mColors[mActiveAxis],0.6f }, 1);
			Utils::renderLine(pos, newPos, { mColors[mActiveAxis],1.0f }, 3);
		}
	}

	void Gizmo::drawScaleGizmo(const Math::Vec3& pos) {
		
	}

	void Gizmo::drawPosGizmo(const Math::Vec3& pos) {
		
	}
}
