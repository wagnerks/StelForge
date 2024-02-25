#include "ComponentsDebug.h"

#include <algorithm>

#include "imgui.h"
#include "imgui_internal.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "core/FileSystem.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/TreeComponent.h"
#include "core/ECSHandler.h"
#include "core/ThreadPool.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/systems/CameraSystem.h"
#include "renderModule/Utils.h"
#include "misc/cpp/imgui_stdlib.h"
#include "myPhysicsEngine/GJK.h"
#include "systemsModule/SystemManager.h"

#include <Jolt/Jolt.h>

#include "Benchmark.h"
#include "componentsModule/ActionComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/PhysicsComponent.h"
#include "componentsModule/ShaderComponent.h"
#include "core/OcTree.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "mathModule/CameraUtils.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/PhysicsSystem.h"

#include "mathModule/ImGuiHelpers.h"

using namespace SFE::Debug;

void ComponentsDebug::init() {
	static bool leftM = false;
	static bool rightM = false;
	onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType event) {
		if (event == CoreModule::InputEventType::PRESS) {
			pressedKeys[key] = true;
		}
		else if (event == CoreModule::InputEventType::RELEASE) {
			pressedKeys[key] = false;
			if (key == CoreModule::InputKey::KEY_0) {
				gizmo.setMode(Render::GizmoMode::NONE);
			}
			else if (key == CoreModule::InputKey::KEY_1) {
				gizmo.setMode(Render::GizmoMode::MOVE);
			}
			else if (key == CoreModule::InputKey::KEY_2) {
				gizmo.setMode(Render::GizmoMode::ROTATE);
			}
			else if (key == CoreModule::InputKey::KEY_3) {
				gizmo.setMode(Render::GizmoMode::SCALE);
			}
		}
	};
	onMouseBtnEvent = [this](Math::DVec2 mPos, CoreModule::MouseButton btn, CoreModule::InputEventType action) {
		if (btn == CoreModule::MouseButton::MOUSE_BUTTON_LEFT && action == CoreModule::InputEventType::PRESS) {
			if (pressedKeys[CoreModule::InputKey::KEY_TAB]) {
				leftM = true;

				auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();
				auto camRay = Math::calcMouseRay(camera, Math::Vec2{static_cast<float>(mPos.x), static_cast<float>(mPos.y)});

				float minDistance = std::numeric_limits<float>::max();

				ECSHandler::getSystem<SystemsModule::OcTreeSystem>()->forEachOctreeInAABB(Math::createFrustum(camera).generateAABB(), [&](SystemsModule::OcTreeSystem::SysOcTree& tree) {
					auto res = tree.findCollisions(camRay.a, camRay.direction, [](const auto& data) {
						return data.data != ecss::INVALID_ID;
					});

					for (auto& [collisionPos, object] : res) {
						auto dist = Math::lengthSquared(collisionPos - camRay.a);
						if (dist < minDistance) {
							minDistance = dist;
							setSelectedId(object.data.getID());
						}
					}
				});

				if (minDistance < std::numeric_limits<float>::max()) {
					if (ECSHandler::registry().getComponent<OutlineComponent>(mSelectedId)) {
						ECSHandler::registry().removeComponent<OutlineComponent>(mSelectedId);
					}
					else {
						ECSHandler::registry().addComponent<OutlineComponent>(mSelectedId);
					}
				}
			}
		}

		if (btn == CoreModule::MouseButton::MOUSE_BUTTON_RIGHT && (action == CoreModule::InputEventType::PRESS || action == CoreModule::InputEventType::REPEAT)) {
			rightM = true;
			if (pressedKeys[CoreModule::InputKey::KEY_LEFT_ALT]) {
				auto bullet = ECSHandler::registry().takeEntity();
				auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();

				auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
				auto pos = cameraTransform->getPos(true);
				auto camRay = Math::calcMouseRay(camera, Math::Vec2{static_cast<float>(mPos.x), static_cast<float>(mPos.y)});

				auto transform = ECSHandler::registry().addComponent<TransformComponent>(bullet, bullet);
				transform->setPos(camRay.a + camRay.direction * 100.f);
				transform->setScale({ 0.1f, 0.1f, 0.1f });
				auto cubeModel = AssetsModule::ModelLoader::instance()->load("models/cube.fbx");
				ECSHandler::registry().addComponent<SFE::ComponentsModule::AABBComponent>(bullet);
				ECSHandler::registry().addComponent<OcTreeComponent>(bullet);
				ECSHandler::registry().addComponentWithInit<ModelComponent>(bullet, [cubeModel](ModelComponent* comp) { comp->init(cubeModel); }, bullet.getID());


				JPH::BodyInterface& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
				auto tr = ECSHandler::registry().getComponent<TransformComponent>(bullet);
				auto aabb = ECSHandler::registry().getComponent<ComponentsModule::AABBComponent>(bullet);


				JPH::BoxShapeSettings cube_shape(JPH::Vec3(10.f, 10.f, 10.f));
				JPH::BodyCreationSettings cube_settings(cube_shape.Create().Get(), JPH::toVec3(pos), JPH::toQuat(tr->getQuaternion()), JPH::EMotionType::Dynamic, Layers::MOVING);

				auto mBodyID = body_interface.CreateAndAddBody(cube_settings, JPH::EActivation::Activate);
				body_interface.SetRestitution(mBodyID, 0.5f);
				ECSHandler::registry().addComponent<PhysicsComponent>(bullet, mBodyID);
				camRay.direction *= 1000000.f;
				body_interface.SetLinearVelocity(mBodyID, { camRay.direction.x, camRay.direction.y, camRay.direction.z });
			}
		}

		if (btn == CoreModule::MouseButton::MOUSE_BUTTON_RIGHT && (action == CoreModule::InputEventType::PRESS || action == CoreModule::InputEventType::REPEAT)) {
			rightM = true;

			if (pressedKeys[CoreModule::InputKey::KEY_LEFT_CONTROL]) {
				auto bullet = ECSHandler::registry().takeEntity();
				auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();

				auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
				auto pos = cameraTransform->getPos(true);

				auto camRay = Math::calcMouseRay(camera, Math::Vec2{static_cast<float>(mPos.x), static_cast<float>(mPos.y)});

				float minDistance = std::numeric_limits<float>::max();
				Math::Vec3 curColision = {};

				ECSHandler::getSystem<SystemsModule::OcTreeSystem>()->forEachOctreeInAABB(Math::createFrustum(camera).generateAABB(), [&](SystemsModule::OcTreeSystem::SysOcTree& tree) {
					auto res = tree.findCollisions(camRay.a, camRay.direction, [](const auto& data) {
						return data.data != ecss::INVALID_ID;
					});

					for (auto& [collisionPos, object] : res) {
						auto dist = Math::lengthSquared(collisionPos - pos);
						if (dist < minDistance) {
							minDistance = dist;
							curColision = collisionPos;
							setSelectedId(object.data.getID());
						}
					}
				});

				auto transform = ECSHandler::registry().addComponent<TransformComponent>(bullet, bullet);
				transform->setPos(curColision + Math::Vec3(0.f, 10.f, 0.f));
				transform->setScale({ 0.1f, 0.1f, 0.1f });
				auto cubeModel = AssetsModule::ModelLoader::instance()->load("models/cube.fbx");
				ECSHandler::registry().addComponent<SFE::ComponentsModule::AABBComponent>(bullet);
				ECSHandler::registry().addComponent<OcTreeComponent>(bullet);
				ECSHandler::registry().addComponentWithInit<ModelComponent>(bullet, [cubeModel](ModelComponent* comp) { comp->init(cubeModel); }, bullet.getID());


				
				auto tr = ECSHandler::registry().getComponent<TransformComponent>(bullet);
				auto aabb = ECSHandler::registry().getComponent<ComponentsModule::AABBComponent>(bullet);


				JPH::BoxShapeSettings cube_shape(JPH::Vec3(10.f, 10.f, 10.f));
				JPH::BodyCreationSettings cube_settings(cube_shape.Create().Get(), JPH::toVec3(pos), JPH::toQuat(tr->getQuaternion()), JPH::EMotionType::Dynamic, Layers::MOVING);

				JPH::BodyInterface& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
				auto mBodyID = body_interface.CreateAndAddBody(cube_settings, JPH::EActivation::Activate);
				body_interface.SetRestitution(mBodyID, 0.5f);
				ECSHandler::registry().addComponent<PhysicsComponent>(bullet, mBodyID);
			}
		}

		if (btn == CoreModule::MouseButton::MOUSE_BUTTON_RIGHT && action == CoreModule::InputEventType::RELEASE) {
			rightM = false;
		}
		if (btn == CoreModule::MouseButton::MOUSE_BUTTON_LEFT && action == CoreModule::InputEventType::RELEASE) {
			leftM = false;
		}
	};

	onMouseEvent = [](Math::DVec2 mPos, Math::DVec2 mouseOffset) {
		if (rightM) {
			
		}
		return;
		if (leftM) {
			auto camera = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();

			auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(camera);
			auto pos = cameraTransform->getPos(true);
			auto mProjection = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera().getID())->getProjection().getProjectionsMatrix();
			auto mView = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera().getID())->getViewMatrix();


			float normalizedX = (2.0f * static_cast<float>(mPos.x)) / Render::Renderer::SCR_WIDTH - 1.0f;
			float normalizedY = 1.0f - (2.0f * static_cast<float>(mPos.y)) / Render::Renderer::SCR_HEIGHT;
			auto clipCoords = Math::Vec4(normalizedX, normalizedY, -1.0, 1.0);
			auto ndc = Math::inverse(mProjection) * clipCoords;
			ndc /= ndc.w;
			auto viewCoords = Math::inverse(mView) * Math::Vec4(Math::Vec3(ndc), 1.0f);
			auto rayDirection = Math::normalize(Math::Vec3(viewCoords) - pos);

			auto octrSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();

			auto mCamFrustum = FrustumModule::createFrustum(mProjection * mView);

			auto aabbOctrees = octrSys->getAABBOctrees(mCamFrustum.generateAABB());

			for (auto& tree : aabbOctrees) {
				if (auto treeIt = octrSys->getOctree(tree)) {
					auto res = treeIt->findCollisions(Math::Vec3(viewCoords), Math::Vec3(rayDirection), [](const auto& data) {
						return data.data != ecss::INVALID_ID;
					});
					for (auto& obj : res) {
						if (!ECSHandler::registry().getComponent<PhysicsComponent>(obj.second.data.getID())) {
							auto tr = ECSHandler::registry().getComponent<TransformComponent>(obj.second.data.getID());
							if (auto aabb = ECSHandler::registry().getComponent<ComponentsModule::AABBComponent>(obj.second.data.getID())) {
								ECSHandler::registry().addComponent<OutlineComponent>(obj.second.data.getID());
								JPH::BodyInterface& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();

								auto pos = tr->getPos(true);
								auto quat = tr->getQuaternion();
								quat.w = 1.f;
								JPH::BoxShapeSettings cube_shape(JPH::toVec3(aabb->aabbs.front().extents));
								JPH::BodyCreationSettings cube_settings(cube_shape.Create().Get(), JPH::toVec3(pos), JPH::toQuat(quat), JPH::EMotionType::Dynamic, Layers::MOVING);

								auto mBodyID = body_interface.CreateAndAddBody(cube_settings, JPH::EActivation::Activate);
								body_interface.SetRestitution(mBodyID, 0.5f);
								ECSHandler::registry().addComponent<PhysicsComponent>(obj.second.data.getID(), mBodyID);
							}
						}
					}
				}
			}
		}
	};
}

void ComponentsDebug::drawTree(const ecss::EntityHandle& entity, ecss::SectorId& selectedID) {
	std::string strid = "";
	if (auto debugData = ECSHandler::registry().getComponent<DebugDataComponent>(entity)) {
		strid = debugData->stringId;
	}

	auto id = entity.getID();
	const auto entityString = std::to_string(id) + ": " + std::string(strid);

	auto treeComp = ECSHandler::registry().getComponent<ComponentsModule::TreeComponent>(entity);
	auto children = treeComp ? treeComp->getChildren() : std::vector<ecss::SectorId>();
	if (!children.empty()) {
		
		int flag = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
		if (selectedID == id) {
			flag = flag | ImGuiTreeNodeFlags_Selected;
		}

		if (ImGui::TreeNodeEx(entityString.c_str(), flag)) {
			if (ImGui::IsItemClicked()) {
				selectedID = id;
				setSelectedId(id);
			}

			for (auto child : children) {
				drawTree(ECSHandler::registry().getEntity(child), selectedID);
			}

			ImGui::TreePop();
		}
		else {
			if (ImGui::IsItemClicked()) {
				selectedID = id;
				setSelectedId(id);
			}
		}
	}
	else {
		if (ImGui::Selectable(entityString.c_str(), selectedID == id)) {
			selectedID = id;
			setSelectedId(id);
		}
	}
}

class SeparatedWindow {
public:
	static void Begin(const char* name, bool* p_open = NULL, std::function<void()> upContent = nullptr, std::function<void()> midContent = nullptr, std::function<void()> belowContent = nullptr, ImGuiWindowFlags flags = 0) {
		if (ImGui::Begin(name, p_open, flags)) {
			if (separatorPos > ImGui::GetWindowHeight() * 0.9f) {
				separatorPos = ImGui::GetWindowHeight() * 0.9f;
			}
			else if (separatorPos < ImGui::GetWindowHeight() * 0.1f) {
				separatorPos = ImGui::GetWindowHeight() * 0.1f;
			}

			ImGui::BeginChild((std::string(name) + "##upper").c_str(), {ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x, separatorPos});
			{
				if (upContent) {
					upContent();
				}
			}
			ImGui::EndChild();

			if (midContent) {
				midContent();
			}

			ImGui::Separator();
			ImGui::Button("--", { ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x, 5.f });

			if (ImGui::IsItemClicked()) {
				clickedSeparator = true;
				mousePos = ImGui::GetMousePos();
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			}
			ImGui::Separator();

			if (ImGui::GetIO().MouseReleased[0]) {
				clickedSeparator = false;
			}

			if (clickedSeparator) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				auto dragDelta = mousePos.y - ImGui::GetMousePos().y;
				mousePos = ImGui::GetMousePos();

				separatorPos -= dragDelta;
			}

			ImGui::BeginChild((std::string(name) + "##down").c_str(), {}, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
			if (belowContent) {
				belowContent();
			}
			
			ImGui::EndChild();
			
		}

		ImGui::End();
	}
private:
	inline static bool clickedSeparator = false;
	inline static float separatorPos = 100.f;
	inline static ImVec2 mousePos = {};
};

void ComponentsDebug::entitiesDebug() {
	FUNCTION_BENCHMARK;
	{
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Debug")) {
				ImGui::Checkbox("entities editor", &editorOpened);
				ImGui::EndMenu();
			}
		}
		ImGui::EndMainMenuBar();
	}

	gizmo.update();

	if (!editorOpened) {
		return;
	}

	mCameraId = ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera();

	if (mSelectedId != ecss::INVALID_ID) {
		auto phisComp = ECSHandler::registry().getComponent<PhysicsComponent>(mSelectedId);
		if (phisComp) {
			auto& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
			Math::Vec3 velocity = {};

			auto transform = ECSHandler::registry().getComponent<TransformComponent>(mCameraId);
			
			if (pressedKeys[CoreModule::InputKey::KEY_UP]) {
				velocity += Math::Vec3{10.f, 0.f, 10.f} * transform->getForward() / transform->getGlobalScale();
			}
			if (pressedKeys[CoreModule::InputKey::KEY_LEFT]) {
				velocity += Math::Vec3{10.f, 0.f, 10.f} * (-transform->getRight()) / transform->getGlobalScale();
			}
			if (pressedKeys[CoreModule::InputKey::KEY_RIGHT]) {
				velocity += Math::Vec3{10.f, 0.f, 10.f} * transform->getRight() / transform->getGlobalScale();
			}
			if (pressedKeys[CoreModule::InputKey::KEY_DOWN]) {
				velocity += Math::Vec3{10.f, 0.f, 10.f} * transform->getBackward() / transform->getGlobalScale();
			}
			if (pressedKeys[CoreModule::InputKey::KEY_LEFT_CONTROL]) {
				velocity += Math::Vec3{0.f, 150.f, 0.f};
			}
			
			body_interface.SetLinearVelocity(phisComp->mBodyID, JPH::toVec3(velocity));
		}
	}

	SeparatedWindow::Begin("Entities Editor", &editorOpened, std::bind(&ComponentsDebug::entitiesTreeGUI, this), std::bind(&ComponentsDebug::entitiesMenuGUI, this), std::bind(&ComponentsDebug::componentGUI, this));
}

void ComponentsDebug::editComponentGui(ComponentsModule::CameraComponent* component) {
	auto isMain = mCameraId == mSelectedId;

	if (ImGui::Checkbox("main", &isMain)) {
		if (isMain) {
			ECSHandler::getSystem<SystemsModule::CameraSystem>()->setCurrentCamera(mSelectedId);
		}
		else {
			ECSHandler::getSystem<SystemsModule::CameraSystem>()->setCurrentCamera(ecss::INVALID_ID);
		}
	}
	/*auto coloredLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
	coloredLines->use();

	coloredLines->setMat4("PVM", PV * Math::Mat4{model});

	Render::Utils::renderCamera();*/
}

void ComponentsDebug::editComponentGui(TransformComponent* component) {
	auto pos = component->getPos();
	auto scale = component->getScale();

	float posV[] = { (float)pos.x, (float)pos.y, (float)pos.z };
	float scaleV[] = { (float)scale.x, (float)scale.y, (float)scale.z };

	if (ImGui::DragFloat3("Pos", posV, 0.1f)) {
		component->setPos({ posV[0], posV[1], posV[2] });
	}

	float rotations[] = { component->getRotate().x, component->getRotate().y, component->getRotate().z };
	if (ImGui::DragFloat3("Rotate", rotations, 0.1f)) {
		component->setRotate({ rotations[0], rotations[1], rotations[2] });
	}

	if (ImGui::DragFloat3("Scale", scaleV, 0.1f)) {
		component->setScale({ scaleV[0], scaleV[1], scaleV[2] });
	}
}

void ComponentsDebug::editComponentGui(ComponentsModule::LightSourceComponent* component) {
	Render::Utils::renderPointLight(component->mNear, component->mRadius, ECSHandler::registry().getComponent<TransformComponent>(mSelectedId)->getPos(true));

	std::vector<std::string> items { "NONE", "DIRECTIONAL", "POINT", "PERSPECTIVE", "WORLD"};
	std::string currentItem = items[static_cast<int>(component->getType())];
	if (ImGui::BeginCombo("Light type", currentItem.c_str())) {
		for (int n = 0; n < items.size(); n++) {
			bool is_selected = (static_cast<int>(component->getType()) == n);
			if (ImGui::Selectable(items[n].c_str(), is_selected)) {
				component->setType(static_cast<ComponentsModule::eLightType>(n));
			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Separator();

	auto intensity = component->getIntensity();
	if (ImGui::DragFloat("intensity", &intensity, 0.01f)) {
		component->setIntensity(intensity);
	}

	auto biasValue = component->getBias();
	if (ImGui::DragFloat("bias", &biasValue, 0.01f)) {
		component->setBias(biasValue);
	}

	float texelSize[2] = { component->getTexelSize().x, component->getTexelSize().y };
	if (ImGui::DragFloat2("texel size", texelSize, 0.01f)) {
		component->setTexelSize({ texelSize[0], texelSize[1] });
	}

	auto samplesCount = component->getSamples();
	if (ImGui::DragInt("samples", &samplesCount, 1, 1)) {
		if (samplesCount < 1) {
			samplesCount = 1;
		}
		component->setSamples(samplesCount);
	}

	float lightColor[3] = { component->getLightColor().r, component->getLightColor().g, component->getLightColor().b };
	if (ImGui::ColorPicker3("light color", lightColor)) {
		component->setLightColor({ lightColor[0], lightColor[1], lightColor[2] });
	}

	ImGui::DragFloat("linear", &component->mLinear, 0.1f);
	ImGui::DragFloat("quadratic", &component->mQuadratic, 0.01f);
	ImGui::DragFloat("near", &component->mNear, 0.1f);
	ImGui::DragFloat("radius", &component->mRadius, 0.1f);
}

void ComponentsDebug::editComponentGui(CascadeShadowComponent* component) {
	if (!component) {
		return;
	}

	auto nameCreator = [](std::string name, int idx) {
		return std::string(name + "##" + std::to_string(idx));
	};

	ImGui::DragFloat2("resolution", component->resolution);

	ImGui::DragFloat("shadows_intensity", &component->shadowIntensity, 0.02f, 0.f, 1.f);
	if (ImGui::TreeNode("cascades")) {
		int i = 0;
		for (auto& cascade : component->cascades) {
			if (ImGui::TreeNode(nameCreator("cascade", i).c_str())) {
				if (ImGui::DragFloat(nameCreator("near", i).c_str(), &component->shadowCascadeLevels[i])) {
					component->markDirty();
				}

				if (ImGui::DragFloat(nameCreator("far", i).c_str(), &component->shadowCascadeLevels[i + 1])) {
					component->markDirty();
				}

				ImGui::Separator();

				ImGui::DragFloat(nameCreator("bias", i).c_str(), &cascade.bias);
				ImGui::DragInt(nameCreator("samples", i).c_str(), &cascade.samples);

				ImGui::DragFloat2(nameCreator("texel size", i).c_str(), cascade.texelSize);

				/*ImGui::DragFloat(nameCreator("z mult near", i).c_str(), &cascade.zMult.x);
				ImGui::DragFloat(nameCreator("z mult far", i).c_str(), &cascade.zMult.y);*/

				ImGui::TreePop();
			}
			i++;
		}
		ImGui::TreePop();
	}


	if (ImGui::Button("save to json")) {
		FileSystem::writeJson("cascadedShadows.json", PropertiesModule::PropertiesSystem::serializeEntity(ECSHandler::registry().getEntity(component->getEntityId())));
	}

}

void ComponentsDebug::editComponentGui(ComponentsModule::ModelComponent* component) {
	if (!component) {
		return;
	}

	auto& modObj = component->getModel(0);

	static bool drawNormales = false;
	static bool drawTangent = false;
	static bool drawBiTangent = false;
	static bool vertexNormals = false;
	static bool drawBones = true;

	ImGui::Checkbox("drawNormals", &drawNormales);
	ImGui::Checkbox("drawTangent", &drawTangent);
	ImGui::Checkbox("drawBiTangent", &drawBiTangent);
	ImGui::Checkbox("drawBones", &drawBones);

	if (ImGui::RadioButton("vertex normales", vertexNormals)) {
		vertexNormals = true;
	}
	if (ImGui::RadioButton("face normales", !vertexNormals)) {
		vertexNormals = false;
	}

	if (drawNormales || drawTangent || drawBiTangent || drawBones) {
		auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(mCameraId);
		auto& cameraRotation = cameraTransform->getQuaternion();
		auto& camPos = cameraTransform->getPos(true);

		auto transform = ECSHandler::registry().getComponent<TransformComponent>(mSelectedId)->getTransform();
		
		//vertex normals
		if (vertexNormals) {
			for (auto& mesh : modObj.meshes) {
				for (int i = 0; i < mesh->mData.vertices.size(); i++) {
					auto pos = mesh->mData.vertices[i].mPosition;
					pos = Math::Vec3(transform * Math::Vec4(pos, 1.f));
					if (distance(camPos, pos) < 25.f) {
						if (drawNormales) {
							//if (dot(viewDir, mesh->mData.vertices[i].mNormal) < 0.f) {
							auto posEnd = pos + mesh->mData.vertices[i].mNormal * 1.5f;
							Render::Utils::renderLine(pos, posEnd, Math::Vec4(1.f, 0.f, 0.f, 1.f));

							//}

						}
						if (drawTangent) {
							//if (dot(viewDir, mesh->mData.vertices[i].mTangent) < 0.f) {
							auto posEnd = pos + mesh->mData.vertices[i].mTangent * 1.5f;
							Render::Utils::renderLine(pos, posEnd, Math::Vec4(0.f, 1.f, 0.f, 1.f));
							//}
						}
						if (drawBiTangent) {
							//if (dot(viewDir, mesh->mData.vertices[i].mBiTangent) < 0.f) {
							auto posEnd = pos + mesh->mData.vertices[i].mBiTangent * 1.5f;
							Render::Utils::renderLine(pos, posEnd, Math::Vec4(0.f, 0.f, 1.f, 1.f));
							//}
						}
					}

				}
			}
		}
		else {
			//face normals
			for (auto& mesh : modObj.meshes) {
				for (int i = 0; i < mesh->mData.indices.size(); i += 3) {

					auto pos = mesh->mData.vertices[mesh->mData.indices[i]].mPosition;
					pos += mesh->mData.vertices[mesh->mData.indices[i + 1]].mPosition;
					pos += mesh->mData.vertices[mesh->mData.indices[i + 2]].mPosition;
					pos /= 3.f;

					if (drawNormales) {
						auto posEnd = pos + mesh->mData.vertices[mesh->mData.indices[i]].mNormal * 5.f;
						Render::Utils::renderLine(pos, posEnd, Math::Vec4(1.f, 0.f, 0.f, 1.f));
					}
					if (drawTangent) {
						auto posEnd = pos + mesh->mData.vertices[mesh->mData.indices[i]].mTangent * 5.f;
						Render::Utils::renderLine(pos, posEnd, Math::Vec4(1.f, 0.f, 0.f, 1.f));
					}
					if (drawBiTangent) {
						auto posEnd = pos + mesh->mData.vertices[mesh->mData.indices[i]].mBiTangent * 5.f;
						Render::Utils::renderLine(pos, posEnd, Math::Vec4(1.f, 0.f, 0.f, 1.f));
					}

				}
			}
		}

		auto& bones = component->armature.bones;
		if (drawBones) {

			if (!bones.empty()) {

				std::function<void(AssetsModule::Bone&, Math::Mat4)> drawBonesF;
				drawBonesF = [&drawBonesF, &bones, &cameraRotation](AssetsModule::Bone& bone, Math::Mat4 globalTransform) {
					auto globalTransfor = globalTransform * bone.transform;
					auto offset = bone.transform == Math::Mat4(1.f) ? bone.offset : Math::Mat4(1.f);

					auto pos = Math::Vec3(globalTransfor * offset * Math::Vec4(Math::Vec3(0.f), 1.f));
					for (auto child : bone.childrenBones) {
						auto childGlobalTransform = globalTransfor * bones[child].transform;
						offset = bones[child].transform == Math::Mat4(1.f) ? bones[child].offset : Math::Mat4(1.f);

						auto posEnd = Math::Vec3(childGlobalTransform * offset * Math::Vec4(Math::Vec3(0.f), 1.f));
						Render::Utils::renderBone(pos, posEnd, Math::Vec4(0.3f, 0.3f, 0.3f, 1.f), cameraRotation, bone.rotation);
						drawBonesF(bones[child], globalTransfor);
					}
				};

				drawBonesF(bones[0], transform * component->armature.transform);
			}
		}

		if (!bones.empty()) {
			std::function<void(AssetsModule::Bone&)> drawBonesTree;
			drawBonesTree = [&drawBonesTree, &bones](AssetsModule::Bone& bone) {

				if (ImGui::TreeNode(("bone " + bone.name).c_str())) {
					if (ImGui::DragFloat3("pos " + bone.name, bone.pos, 0.1f)) {
						bone.transform = Math::translate(Math::Mat4{1.f}, bone.pos)* bone.rotation.toMat4()* Math::scale(Math::Mat4{ 1.f }, bone.scale);
					}
					if (ImGui::DragFloat4("quat " + bone.name, bone.rotation, 0.1f)) {
						bone.transform = Math::translate(Math::Mat4{1.f}, bone.pos)* bone.rotation.toMat4()* Math::scale(Math::Mat4{ 1.f }, bone.scale);
					}
					auto rotat = degrees(bone.rotation.toEuler());
					if (ImGui::DragFloat3("rotat " + bone.name, rotat)) {
						bone.rotation.eulerToQuaternion(rotat);
						bone.transform = Math::translate(Math::Mat4{1.f}, bone.pos)* bone.rotation.toMat4()* Math::scale(Math::Mat4{ 1.f }, bone.scale);
					}
					if (ImGui::DragFloat3("scale " + bone.name, bone.scale)) {
						bone.transform = Math::translate(Math::Mat4{1.f}, bone.pos)* bone.rotation.toMat4()* Math::scale(Math::Mat4{ 1.f }, bone.scale);
					}

					for (auto child : bone.childrenBones) {
						drawBonesTree(bones[child]);
					}

					ImGui::TreePop();
				}


			};

			drawBonesTree(bones[0]);
		}

	}


	if (ImGui::Button("recalculate normals")) {
		auto model = AssetsModule::ModelLoader::instance()->load(component->mPath);
		model->recalculateNormals();
		component->init(model);
	}
	if (ImGui::Button("recalculate normals smooth")) {
		auto model = AssetsModule::ModelLoader::instance()->load(component->mPath);
		model->recalculateNormals(true);
		component->init(model);
	}
	auto& modelobj = component->getModel(0);
	if (ImGui::Button("update bind")) {
		if (auto model = AssetsModule::AssetsManager::instance()->getAsset<AssetsModule::Model>(component->mPath)) {
			model->bindMeshes();
		}
	}
	int i = 0;
	for (auto& lod : modelobj.meshes) {
		auto treeLabel = "LOD " + std::to_string(i) + "##meshLod";
		if (ImGui::TreeNode(std::to_string(i).c_str())) {

			ImGui::Text("vertices: %zu", lod->mData.vertices.size());
			ImGui::Text("indices: %zu", lod->mData.indices.size());
			ImGui::Spacing();

			static std::string diffusePath = "";
			ImGui::InputText("diffusePath", &diffusePath);
			if (ImGui::Button("load##diffuse")) {
				lod->mMaterial[AssetsModule::DIFFUSE] = AssetsModule::TextureHandler::instance()->loadTexture(diffusePath);
			}

			static std::string normalPath = "";
			ImGui::InputText("normalPath", &normalPath);
			if (ImGui::Button("load##normalPath")) {
				lod->mMaterial[AssetsModule::NORMALS] = AssetsModule::TextureHandler::instance()->loadTexture(normalPath);
			}

			static std::string specularPath = "";
			ImGui::InputText("specularPath", &specularPath);
			if (ImGui::Button("load##specularPath")) {
				lod->mMaterial[AssetsModule::SPECULAR] = AssetsModule::TextureHandler::instance()->loadTexture(specularPath);
			}

			ImGui::Text("diffuse:");
			if (auto texture = lod->mMaterial.tryGetTexture(AssetsModule::DIFFUSE); texture && texture->isValid()) {
				ImGui::Image(reinterpret_cast<ImTextureID>(texture->mId), { 200.f,200.f });

			}
			else {
				ImGui::SameLine();
				ImGui::Text("none");
			}

			ImGui::Text("specular:");
			if (auto texture = lod->mMaterial.tryGetTexture(AssetsModule::SPECULAR); texture && texture->isValid()) {
				ImGui::Image(reinterpret_cast<ImTextureID>(texture->mId), { 200.f,200.f });
			}
			else {
				ImGui::SameLine();
				ImGui::Text("none");
			}

			ImGui::Text("normal:");
			if (auto texture = lod->mMaterial.tryGetTexture(AssetsModule::NORMALS); texture && texture->isValid()) {
				ImGui::Image(reinterpret_cast<ImTextureID>(texture->mId), { 200.f,200.f });
			}
			else {
				ImGui::SameLine();
				ImGui::Text("none");
			}


			ImGui::TreePop();
		}
		i++;
	}
}


struct ShaderVariable {
	enum class ShaderVariableType {
		INT, FLOAT, VEC2, VEC3, VEC4, MAT3, MAT4, SAMPLER_2D, UNKNOWN
	};

	inline static ShaderVariableType stringToShaderVariableType(std::string_view stringType) {
		if (stringType == "int") {
			return ShaderVariableType::INT;
		}
		else if (stringType == "float") {
			return ShaderVariableType::FLOAT;
		}
		else if (stringType == "vec2") {
			return ShaderVariableType::VEC2;
		}
		else if (stringType == "vec3") {
			return ShaderVariableType::VEC3;
		}
		else if (stringType == "vec4") {
			return ShaderVariableType::VEC4;
		}
		else if (stringType == "mat3") {
			return ShaderVariableType::MAT3;
		}
		else if (stringType == "mat4") {
			return ShaderVariableType::MAT4;
		}
		else if (stringType == "sampler2D") {
			return ShaderVariableType::SAMPLER_2D;
		}

		return ShaderVariableType::UNKNOWN;
	}

	ShaderVariable(std::string_view type, std::string_view name, size_t arraySize) : type(stringToShaderVariableType(type)), name(name), typeString(type), arraySize(arraySize) {}

	ShaderVariableType type;
	std::string name;
	std::string typeString;
	size_t arraySize = 0;
};

struct ShaderStruct {
	std::string name;
	std::vector<ShaderVariable> variables;
};

void ComponentsDebug::editComponentGui(ComponentsModule::ShaderComponent* component) {
	if (!component) {
		return;
	}

	ImGui::Text("shader path, vertex / fragment");

	ImGui::InputText("v##vertexShaderPath", &component->shaderPathVertex);
	ImGui::InputText("f##fragmentShaderPath", &component->shaderPathFragment);

	if (ImGui::Button("load")) {
		auto shader = SHADER_CONTROLLER->loadVertexFragmentShader(component->shaderPathVertex, component->shaderPathFragment);
		SHADER_CONTROLLER->deleteShader(shader);
		shader = SHADER_CONTROLLER->loadVertexFragmentShader(component->shaderPathVertex, component->shaderPathFragment);
		component->shaderId = shader->getID();
	}

	auto shader = dynamic_cast<ShaderModule::Shader*>(SHADER_CONTROLLER->getShader(component->shaderId));
	if (shader) {
		if (ImGui::Button("fill shader uniforms")) {
			std::vector<ShaderVariable> uniforms;
			std::vector<ShaderStruct> structs;
			auto findUniforms = [&uniforms, &structs](std::string code) {
				std::string word = "";

				std::vector<std::string> words;
				bool isStruct = false;
				bool closedStruct = false;

				bool comment = false;

				int longComment = 0;
				char prevSymbol = ' ';
				std::map<std::string, std::string> variables;
				for (const auto& codeSymbol : code) {
					if (codeSymbol == '\n') {
						comment = false;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (prevSymbol == '/' && codeSymbol == '*') {
						longComment++;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (prevSymbol == '*' && codeSymbol == '/') {
						longComment--;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (prevSymbol == '/' && codeSymbol == '/') {
						comment = true;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (comment || longComment) { //ignore comments
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == '=') {
						if (word != "") {
							words.push_back(word);
						}

						words.push_back("=");
						word = "";
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == ' ') {
						if (word != "") {
							words.push_back(word);
						}
						word = "";
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == '{') {
						if (words[0] == "struct") {
							structs.push_back({ words[1],{} });

							isStruct = true;
							words.clear();
						}
						word = "";
						prevSymbol = codeSymbol;
						continue;
					}
					else if (codeSymbol == '}') {
						if (isStruct) {
							closedStruct = true;
						}
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == ';') {
						if (word != "") {
							words.push_back(word);
						}
						word = "";

						if (!words.empty()) {
							if (isStruct || words[0] == "uniform") {
								if (words[0] == "uniform") {
									words.erase(words.begin());
								}

								auto type = words[0];
								auto name = words[1];

								for (auto i = 2u; i < words.size(); i++) {
									if (words[i] == "=") {
										break;
									}

									words[1] += words[i]; //merge name with '[' 'agea' ']' and ignore spaces
								}

								auto arrayBracket = name.find_first_of('[');
								std::string arrayInfo = "";
								size_t arraySize = 1;

								if (arrayBracket != std::string::npos) {
									arrayInfo = name.substr(arrayBracket + 1, name.size() - arrayBracket - 2);
									name = name.substr(0, arrayBracket);
								}

								if (!arrayInfo.empty()) {
									if (auto& varvalue = variables[arrayInfo]; !varvalue.empty()) {
										arraySize = std::atoi(varvalue.c_str());
									}
									else {
										arraySize = std::atoi(arrayInfo.c_str());
									}
								}

								if (isStruct) {
									structs.back().variables.push_back({ type, name, arraySize });
								}
								else {
									uniforms.push_back({ type, name, arraySize });
								}


								if (closedStruct) {
									isStruct = false;
									closedStruct = false;
								}

							}

							for (size_t i = 0; i < words.size(); i++) {
								if (words[i] == "=") {
									variables[words[i - 1]] = words[i + 1];
								}
							}

							words.clear();
						}
						prevSymbol = codeSymbol;
						continue;
					}

					word += codeSymbol;
					prevSymbol = codeSymbol;
				}
			};


			/*findUniforms(shader->vertexCode);
			findUniforms(shader->fragmentCode);
			std::function<void(const std::vector<ShaderVariable>& uniforms, ComponentsModule::ShaderVariablesStruct& variables)> fillComponentUniforms;
			fillComponentUniforms = [&fillComponentUniforms, &structs](const std::vector<ShaderVariable>& uniforms, ComponentsModule::ShaderVariablesStruct& variables) {
				for (auto& uniform : uniforms) {
					switch (uniform.type) {
					case ShaderVariable::ShaderVariableType::INT:
						variables.integerUniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::FLOAT:
						variables.floatUniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::VEC2:
						variables.vec2Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::VEC3:
						variables.vec3Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::VEC4:
						variables.vec4Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::MAT3:
						variables.mat3Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::MAT4:
						variables.mat4Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::SAMPLER_2D:
						break;
					case ShaderVariable::ShaderVariableType::UNKNOWN:
						for (auto structShader : structs) {
							if (structShader.name == uniform.typeString) {
								variables.structUniforms[uniform.name].value.resize(uniform.arraySize);
								for (auto& str : variables.structUniforms[uniform.name].value) {
									fillComponentUniforms(structShader.variables, str);
								}

								break;
							}
						}
						break;
					default:;
					}
				}
			};

			fillComponentUniforms(uniforms, component->variables);*/

		}

		//std::function<void(ComponentsModule::ShaderVariablesStruct&)> drawShaderVirables = nullptr;
		//drawShaderVirables = [&drawShaderVirables](ComponentsModule::ShaderVariablesStruct& variables) {
		//	for (auto& variable : variables.floatUniforms.data) {
		//		if (variable.value.size() > 1) {
		//			for (auto idx = 0u; idx < variable.value.size(); idx++) {
		//				ImGui::DragFloat((variable.name + "[" + std::to_string(idx) + "]").c_str(), &variable.value[idx]);
		//			}
		//		}
		//		else {
		//			ImGui::DragFloat((variable.name).c_str(), &variable.value.front());
		//		}
		//	}

		//	for (auto& variable : variables.integerUniforms.data) {
		//		if (variable.value.size() > 1) {
		//			for (auto idx = 0u; idx < variable.value.size(); idx++) {
		//				ImGui::DragInt((variable.name + "[" + std::to_string(idx) + "]").c_str(), &variable.value[idx]);
		//			}
		//		}
		//		else {
		//			ImGui::DragInt((variable.name).c_str(), &variable.value.front());
		//		}
		//	}


		//	for (auto& str : variables.structUniforms.data) {
		//		if (ImGui::TreeNode(str.name.c_str())) {
		//			size_t i = 0;
		//			for (auto& val : str.value) {
		//				if (ImGui::TreeNode((str.name + "[" + std::to_string(i) + "]").c_str())) {
		//					drawShaderVirables(val);
		//					ImGui::TreePop();
		//				}
		//				i++;
		//			}

		//			ImGui::TreePop();
		//		}


		//	}

		//	/*for (auto variable : variables.mat4Uniforms.data) {
		//		if (variable.value.size() > 1) {
		//			for (auto idx = 0u; idx < variable.value.size(); idx++) {
		//				ImGui::DragFloat4((variable.name + "[" + std::to_string(idx) + "]").c_str(), &variable.value[idx]);
		//			}
		//		}
		//		else {
		//			ImGui::DragInt((variable.name).c_str(), &variable.value.front());
		//		}
		//	}*/
		//};

		//drawShaderVirables(component->variables);

		auto& vertex = shader->vertexCode;
		auto& fragment = shader->fragmentCode;

		ImGui::InputTextMultiline("vertex", &vertex);
		ImGui::InputTextMultiline("fragment", &fragment);

		if (ImGui::Button("compile")) {
			FileSystem::writeFile(shader->getVertexPath(), vertex);
			FileSystem::writeFile(shader->getFragmentPath(), fragment);
			SHADER_CONTROLLER->recompileShader(shader);
			component->shaderId = shader->getID();
		}

		if (ImGui::Button("apply")) {
			component->variables.apply(shader);
		}
	}
}

void ComponentsDebug::editComponentGui(ComponentsModule::PhysicsComponent* component) {
	
}

void ComponentsDebug::editComponentGui(ComponentsModule::AnimationComponent* component) {
	auto modelPath = ECSHandler::registry().getComponent<ModelComponent>(mSelectedId)->mPath;
	if (auto modelOO = AssetsModule::AssetsManager::instance()->getAsset<AssetsModule::Model>(modelPath)) {
		for (const auto& animation : modelOO->getAnimations()) {
			if (ImGui::Button(animation.mName.c_str())) {
				component->mCurrentAnimation = &animation;
			}
		}
	}

	if (component->mCurrentAnimation) {
		ImGui::Separator();
		if (ImGui::Button(("stop##" + component->mCurrentAnimation->mName).c_str())) {
			component->mPlay = false;
		}
		if (ImGui::Button(("continue##" + component->mCurrentAnimation->mName).c_str())) {
			component->mPlay = true;
		}

		if (auto anim = component->mCurrentAnimation) {
			auto time = component->mCurrentTime;
			if (ImGui::SliderFloat(("time##" + component->mCurrentAnimation->mName).c_str(), &time, 0.f, anim->getDuration())) {
				component->mCurrentTime = time;
				component->step = true;
			}
		}
	}
}

void ComponentsDebug::editComponentGui(ComponentsModule::AABBComponent* component) {
	static bool drawAABB = false;
	ImGui::Checkbox("draw aabb", &drawAABB);
	if (drawAABB) {
		for (auto aabb : component->aabbs) {
			auto extents = aabb.extents;
			auto center = aabb.center;

			Render::Utils::renderCube(
				Math::Vec3(center - extents),
				Math::Vec3(center + extents),
				{ 1.f }, {}, { 1.f,1.f,1.f,1.f }
			);
		}
	}
}

void ComponentsDebug::entitiesTreeGUI() {
	size_t i = 0;
	auto entities = ECSHandler::registry().getAllEntities();
	for (auto entityIt : entities) {
		if (auto treeComp = ECSHandler::registry().getComponent<ComponentsModule::TreeComponent>(entityIt)) {
			if (treeComp->getParent() != ecss::INVALID_ID) {
				continue;
			}
		}

		drawTree(entityIt, mSelectedId);
		i++;
		if (i > 1000) {
			break;
		}
	}
}

void ComponentsDebug::componentGUI() {
	auto& ecsRegistry = ECSHandler::registry();
	if (!ecsRegistry.isEntity(mSelectedId)) {
		return;
	}

	if (auto debugData = ECSHandler::registry().getComponent<DebugDataComponent>(mSelectedId)) {
		ImGui::Text("entity: \"%s\"", debugData->stringId.c_str());
		ImGui::SameLine();
	}
	
	ImGui::Text("id: %zu", mSelectedId);
	ImGui::Separator();


	{
		bool isDrawable = ecsRegistry.getComponent<IsDrawableComponent>(mSelectedId);
		if (ImGui::Checkbox("isDrawable", &isDrawable)) {
			if (isDrawable) {
				ecsRegistry.addComponent<IsDrawableComponent>(mSelectedId);

				auto tree = ecsRegistry.getComponent<ComponentsModule::TreeComponent>(mSelectedId);
				if (tree) {
					for (auto node : tree->getAllNodes()) {
						ecsRegistry.addComponent<IsDrawableComponent>(node);
					}
				}

			}
			else {
				ecsRegistry.removeComponent<IsDrawableComponent>(mSelectedId);
				auto tree = ecsRegistry.getComponent<ComponentsModule::TreeComponent>(mSelectedId);
				if (tree) {
					std::vector<ecss::SectorId> entities;

					for (auto node : tree->getAllNodes()) {
						if (ecsRegistry.getComponent<IsDrawableComponent>(node)) {
							entities.emplace_back(node);
						}
					}

					ecsRegistry.removeComponent<IsDrawableComponent>(entities);
				}
			}
		}
	}


	bool outline = ecsRegistry.getComponent<OutlineComponent>(mSelectedId);
	if (ImGui::Checkbox("outline", &outline)) {
		if (outline) {
			ecsRegistry.addComponent<OutlineComponent>(mSelectedId);
		}
		else {
			ecsRegistry.removeComponent<OutlineComponent>(mSelectedId);
		}
	}
	ImGui::Separator();


	componentEditImpl<TransformComponent>(mSelectedId);
	componentEditImpl<ModelComponent>(mSelectedId);

	//componentEditImpl<LodComponent>(mSelectedId);
	//componentEditImpl<MaterialComponent>(mSelectedId);
	//componentEditImpl<FrustumComponent>(mSelectedId);

	componentEditImpl<CameraComponent>(mSelectedId);
	componentEditImpl<LightSourceComponent>(mSelectedId);
	componentEditImpl<CascadeShadowComponent>(mSelectedId);

	componentEditImpl<ShaderComponent>(mSelectedId);
	componentEditImpl<PhysicsComponent>(mSelectedId);
	componentEditImpl<ComponentsModule::AnimationComponent>(mSelectedId);
	componentEditImpl<ComponentsModule::AABBComponent>(mSelectedId);
}

void ComponentsDebug::entitiesMenuGUI() {
	static ecss::SectorId prevId = std::numeric_limits<ecss::SectorId>::max();
	if (prevId != mSelectedId) {
		if (prevId != std::numeric_limits<ecss::SectorId>::max()) {
			ECSHandler::registry().removeComponent<OutlineComponent>(prevId);
		}

		ECSHandler::registry().addComponent<OutlineComponent>(mSelectedId);
	}

	prevId = mSelectedId;

	auto& compManager = ECSHandler::registry();
	if (ImGui::Button("add node")) {
		auto createdEntity = ECSHandler::registry().takeEntity();
		if (compManager.isEntity(mSelectedId)) {
			compManager.addComponent<TreeComponent>(createdEntity.getID(), createdEntity.getID());
			compManager.addComponent<TreeComponent>(mSelectedId, mSelectedId)->addChildEntity(createdEntity.getID());
		}
		compManager.addComponent<TransformComponent>(createdEntity, createdEntity.getID());
	}

	ImGui::SameLine();
	if (ImGui::Button("DELETE")) {
		compManager.destroyEntity(mSelectedId);
		ImGui::EndChild();
		ImGui::End();
	}

	ImGui::SameLine();

	constexpr std::array<std::string_view, 9> components {
		PropertiesModule::TypeName<TransformComponent>::name(),
		PropertiesModule::TypeName<LightSourceComponent>::name(),
		PropertiesModule::TypeName<ComponentsModule::ActionComponent>::name(),
		PropertiesModule::TypeName<ComponentsModule::AnimationComponent>::name(),
		PropertiesModule::TypeName<CameraComponent>::name(),
		"ph_box",
		"ph_sphere",
		"ph_capsule",
		"ph_floor",
	};
	
	static std::string_view selected = components[0];

	if (ImGui::BeginCombo("##comps", "component")) {
		for (int n = 0; n < components.size(); n++) {
			bool isSelected = (selected == components[n]);
			if (ImGui::Selectable(components[n].data(), isSelected)) {
				selected = components[n];
				if (compManager.isEntity(mSelectedId)) {
					if (selected == "ph_capsule") {
						auto& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
						auto tr = ECSHandler::registry().getComponent<TransformComponent>(mSelectedId);
						auto pos = tr->getPos(true);
						auto quat = tr->getQuaternion();

						JPH::BodyCreationSettings sphere_settings(new JPH::CapsuleShape(100.f, 100.f), JPH::toVec3(pos), JPH::toQuat(quat), JPH::EMotionType::Dynamic, Layers::MOVING);
						auto mBodyID = body_interface.CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);
						body_interface.SetRestitution(mBodyID, 0.5f);
						ECSHandler::registry().addComponent<PhysicsComponent>(mSelectedId, mBodyID);
					}
					else if (selected == "ph_sphere") {
						auto& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
						auto tr = ECSHandler::registry().getComponent<TransformComponent>(mSelectedId);
						auto pos = tr->getPos(true);
						auto quat = tr->getQuaternion();

						JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(100.f), JPH::toVec3(pos), JPH::toQuat(quat), JPH::EMotionType::Dynamic, Layers::MOVING);
						auto mBodyID = body_interface.CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);
						body_interface.SetRestitution(mBodyID, 0.5f);
						ECSHandler::registry().addComponent<PhysicsComponent>(mSelectedId, mBodyID);
					}
					else if (selected == "ph_box") {
						auto& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
						auto tr = ECSHandler::registry().getComponent<TransformComponent>(mSelectedId);
						auto aabb = ECSHandler::registry().getComponent<ComponentsModule::AABBComponent>(mSelectedId);

						auto pos = tr->getPos(true);
						auto quat = tr->getQuaternion();
						JPH::BoxShapeSettings cube_shape(JPH::toVec3(aabb->aabbs.front().extents));
						JPH::BodyCreationSettings cube_settings(cube_shape.Create().Get(), JPH::toVec3(pos), JPH::toQuat(quat), JPH::EMotionType::Dynamic, Layers::MOVING);

						auto mBodyID = body_interface.CreateAndAddBody(cube_settings, JPH::EActivation::Activate);
						body_interface.SetRestitution(mBodyID, 0.5f);
						ECSHandler::registry().addComponent<PhysicsComponent>(mSelectedId, mBodyID);
					}
					else if (selected == "ph_floor") {
						JPH::BodyInterface& body_interface = ECSHandler::getSystem<SFE::SystemsModule::Physics>()->physics_system->GetBodyInterface();
						auto tr = ECSHandler::registry().getComponent<TransformComponent>(mSelectedId);
						tr->setScale({ 100.f, 0.05f, 100.f });
						auto pos = tr->getPos(true);
						auto quat = tr->getQuaternion();
						quat.w = 1.f;
						JPH::BoxShapeSettings cube_shape(JPH::Vec3(10000.0f, 5.0f, 10000.0f));
						JPH::BodyCreationSettings cube_settings(cube_shape.Create().Get(), JPH::toVec3(pos), JPH::toQuat(quat), JPH::EMotionType::Static, Layers::NON_MOVING);

						auto mBodyID = body_interface.CreateAndAddBody(cube_settings, JPH::EActivation::Activate);
						body_interface.SetRestitution(mBodyID, 0.5f);
						ECSHandler::registry().addComponent<PhysicsComponent>(mSelectedId, mBodyID);
					}
					else if (selected == PropertiesModule::TypeName<LightSourceComponent>::name()) {
						compManager.addComponent<LightSourceComponent>(mSelectedId, mSelectedId, ComponentsModule::eLightType::POINT);
					}
					else if (selected == PropertiesModule::TypeName<ComponentsModule::ActionComponent>::name()) {
						compManager.addComponent<ComponentsModule::ActionComponent>(mSelectedId);
					}
					else if (selected == PropertiesModule::TypeName<ComponentsModule::AnimationComponent>::name()) {
						compManager.addComponent<ComponentsModule::AnimationComponent>(mSelectedId);
					}
					else if (selected == PropertiesModule::TypeName<CameraComponent>::name()) {
						ECSHandler::registry().addComponent<CameraComponent>(mSelectedId, mSelectedId, 45.f, static_cast<float>(Render::Renderer::SCR_WIDTH) / static_cast<float>(Render::Renderer::SCR_HEIGHT), Render::Renderer::nearDistance, Render::Renderer::drawDistance);
					}
				}
			}

			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}


	if (ImGui::Button("copy")) {
		auto createdEntity = ECSHandler::registry().takeEntity();
		ECSHandler::registry().copyComponentToEntity(createdEntity.getID(), ECSHandler::registry().getComponent<OcTreeComponent>(mSelectedId));
		ECSHandler::registry().copyComponentToEntity(createdEntity.getID(), ECSHandler::registry().getComponent<ModelComponent>(mSelectedId));
		ECSHandler::registry().copyComponentToEntity(createdEntity.getID(), ECSHandler::registry().getComponent<ComponentsModule::AABBComponent>(mSelectedId));

		ECSHandler::registry().addComponent<TransformComponent>(createdEntity.getID(), createdEntity.getID());
	}
}

template<typename CompType>
void ComponentsDebug::componentEditImpl(ecss::EntityId id) {
	auto component = ECSHandler::registry().getComponent<CompType>(id);
	if (!component) {
		return;
	}
	
	if (!ImGui::TreeNode(PropertiesModule::TypeName<CompType>::name().data())) {
		return;
	}

	editComponentGui(component);

	ImGui::TreePop();
}

void ComponentsDebug::setSelectedId(ecss::EntityId id) {
	mSelectedId = id;
	gizmo.setEntity(mSelectedId);
}
