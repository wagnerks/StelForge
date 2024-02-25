#include "SceneGridFloor.h"

#include "BlendStack.h"
#include "CapabilitiesStack.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"

using namespace SFE::Render;

SceneGridFloor::SceneGridFloor() {
	init();
}
SceneGridFloor::~SceneGridFloor() {	
	SHADER_CONTROLLER->deleteShader(floorShaderHash);
}

void SceneGridFloor::init() {
	floorShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/floorGrid.vs", "shaders/floorGrid.fs");
	if (!floorShader) {
		return;
	}

	floorShaderHash = floorShader->getHash();

	floorShader->use();
	transform = Math::translate(transform, Math::Vec3(0.f));

	Math::Vec3 vertices[] = {
		{ 1.0, 0.f, -1.0}, //far right
		{-1.0, 0.f, -1.0},//far left
		{-1.0, 0.f,  1.0}, //near left

		{ 1.0, 0.f, -1.0}, //far right
		{-1.0, 0.f,  1.0},//near left
		{ 1.0, 0.f,  1.0}, //near right
	};

	VAO.generate();
	VAO.bind();

	VBO.bind();
	VBO.allocateData(6, STATIC_DRAW, &vertices);

	VAO.addAttribute<Math::Vec3>(0, 3, FLOAT, false);

	Buffer::bindDefaultBuffer(ARRAY_BUFFER);
	VertexArray::bindDefault();
}

void SceneGridFloor::draw() {
	if (VAO.getID() == 0) {
		return;
	}

	floorShader->use();
	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(camera);

	floorShader->setMat4("PVM", renderData.current.PV * transform);
    floorShader->setMat4("PV", renderData.current.PV);
	floorShader->setMat4("model", transform);
	
	floorShader->setVec3("cameraPos", Math::Vec3{renderData.mCameraPos});

	floorShader->setFloat("far", cameraComp->getProjection().getFar());
	floorShader->setFloat("near", cameraComp->getProjection().getNear());


	VAO.bind();
	
	CapabilitiesStack::push(CULL_FACE, false);
	CapabilitiesStack::push(BLEND, true);
	BlendFuncStack::push({ SRC_ALPHA, ONE_MINUS_SRC_ALPHA });

	Render::Renderer::drawArrays(TRIANGLES, 6);

	CapabilitiesStack::pop();
	CapabilitiesStack::pop();
	BlendFuncStack::pop();

	VertexArray::bindDefault();
}
