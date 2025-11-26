#include "Skybox.h"

#include "assetsModule/TextureHandler.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "glWrapper/Depth.h"
#include "glWrapper/Draw.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/SystemManager.h"

using namespace SFE::Render;

Skybox::Skybox(std::string_view path) : skyboxPath(path) {}

Skybox::~Skybox() {
	Engine::instance()->rhi()->destroyBuffer(skyboxVBO);
	Engine::instance()->rhi()->destroyTexture(cubemapTex);
}

void Skybox::init() {
	
	//skyboxShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/skybox.vs", "shaders/skybox.fs");
	//if (!skyboxShader) {
	//	return;
	//}

	//skyboxShader->use();
	//skyboxShader->setUniform("skybox", 16);
	//skyboxShader->setUniform("projection", ECSHandler::registry().pinComponent<const CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection().getProjectionsMatrix());

	////cubemapTex = AssetsModule::TextureHandler::instance()->loadCubemapTexture(skyboxPath)->texture.mId; //todo

	//if (cubemapTex.id == UINT32_MAX) {
	//	assert(false && "can't load skybox texture");
	//	return;
	//}

	//float skyboxVertices[] = {
	//	// positions          
	//	-1.0f,  1.0f, -1.0f,
	//	-1.0f, -1.0f, -1.0f,
	//	 1.0f, -1.0f, -1.0f,
	//	 1.0f, -1.0f, -1.0f,
	//	 1.0f,  1.0f, -1.0f,
	//	-1.0f,  1.0f, -1.0f,

	//	-1.0f, -1.0f,  1.0f,
	//	-1.0f, -1.0f, -1.0f,
	//	-1.0f,  1.0f, -1.0f,
	//	-1.0f,  1.0f, -1.0f,
	//	-1.0f,  1.0f,  1.0f,
	//	-1.0f, -1.0f,  1.0f,

	//	 1.0f, -1.0f, -1.0f,
	//	 1.0f, -1.0f,  1.0f,
	//	 1.0f,  1.0f,  1.0f,
	//	 1.0f,  1.0f,  1.0f,
	//	 1.0f,  1.0f, -1.0f,
	//	 1.0f, -1.0f, -1.0f,

	//	-1.0f, -1.0f,  1.0f,
	//	-1.0f,  1.0f,  1.0f,
	//	 1.0f,  1.0f,  1.0f,
	//	 1.0f,  1.0f,  1.0f,
	//	 1.0f, -1.0f,  1.0f,
	//	-1.0f, -1.0f,  1.0f,

	//	-1.0f,  1.0f, -1.0f,
	//	 1.0f,  1.0f, -1.0f,
	//	 1.0f,  1.0f,  1.0f,
	//	 1.0f,  1.0f,  1.0f,
	//	-1.0f,  1.0f,  1.0f,
	//	-1.0f,  1.0f, -1.0f,

	//	-1.0f, -1.0f, -1.0f,
	//	-1.0f, -1.0f,  1.0f,
	//	 1.0f, -1.0f, -1.0f,
	//	 1.0f, -1.0f, -1.0f,
	//	-1.0f, -1.0f,  1.0f,
	//	 1.0f, -1.0f,  1.0f
	//};

	//VertexArrayDesc vaoDesc;
	//BufferDesc vboDesc;
	//vboDesc.size = sizeof(skyboxVertices);
	//vboDesc.usage = BufferUsage::Vertex;
	//vboDesc.cpuAccess = CpuAccess::None;

	//vaoDesc.stride = 3 * sizeof(float);
	//vaoDesc.vertexBuffer = skyboxVBO = Engine::instance()->rhi()->createBuffer(vboDesc, skyboxVertices);
	//vaoDesc.attributes.push_back({ 0, VertexFormat::Float3, 0, false }); // pos.xyz packed
	//skyboxVAO = Engine::instance()->rhi()->createVertexArray(vaoDesc);
}

void Skybox::draw() {
	/*skyboxShader->use();

	auto view = ECSHandler::registry().pinComponent<const TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getViewMatrix();
	skyboxShader->setUniform("view", Math::Mat4(Math::Mat3{view}));
	GLW::DepthFuncStack::push(GLW::DepthFunc::LEQUAL);

	Engine::instance()->rhi()->bindTexture(16, cubemapTex);

	Engine::instance()->rhi()->bindVertexArray(skyboxVAO);
	GLW::drawVerticesW(GLW::TRIANGLES, 36);
	GLW::DepthFuncStack::pop();
	Engine::instance()->rhi()->bindVertexArray({});*/
}
