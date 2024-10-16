#include "SceneGridFloor.h"

#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "glWrapper/BlendStack.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Draw.h"
#include "glWrapper/VertexArray.h"
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
		{ 1.f, 0.f, -1.f}, //far right
		{-1.f, 0.f, -1.f},//far left
		{-1.f, 0.f,  1.f}, //near left

		{ 1.f, 0.f, -1.f}, //far right
		{-1.f, 0.f,  1.f},//near left
		{ 1.f, 0.f,  1.f}, //near right
	};

	VAO.generate();
	VAO.bind();
	VBO.generate();
	VBO.bind();
	VBO.allocateData(6*3, vertices);

	VAO.addAttribute<Math::Vec3>(0, 3, GLW::AttributeFType::FLOAT, false);

	GLW::Buffer<GLW::ARRAY_BUFFER>::bindDefaultBuffer();
	GLW::VertexArray::bindDefault();
}

void SceneGridFloor::draw() {
	if (VAO.getID() == 0) {
		return;
	}

	floorShader->use();
	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(camera);

	floorShader->setUniform("PVM", renderData.current.PV * transform);
    floorShader->setUniform("PV", renderData.current.PV);
	floorShader->setUniform("model", transform);
	
	floorShader->setUniform("cameraPos", Math::Vec3{renderData.mCameraPos});

	floorShader->setUniform("far", cameraComp->getProjection().getFar());
	floorShader->setUniform("near", cameraComp->getProjection().getNear());


	VAO.bind();

	GLW::CapabilitiesStack<GLW::CULL_FACE>::push(false);
	GLW::CapabilitiesStack<GLW::BLEND>::push(true);
	GLW::BlendFuncStack::push({GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });

	GLW::drawVertices(GLW::TRIANGLES, VAO.getID(), 6);

	GLW::CapabilitiesStack<GLW::CULL_FACE>::pop();
	GLW::CapabilitiesStack<GLW::BLEND>::pop();
	GLW::BlendFuncStack::pop();

	GLW::VertexArray::bindDefault();
}
