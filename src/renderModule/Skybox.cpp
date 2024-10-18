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
	if (cubemapTex != -1) {
		//AssetsModule::TextureHandler::instance()->deleteTexture(); TODO
		glDeleteTextures(1, &cubemapTex);
	}

	SHADER_CONTROLLER->deleteShader(skyboxShader);
}

void Skybox::init() {
	skyboxShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/skybox.vs", "shaders/skybox.fs");
	if (!skyboxShader) {
		return;
	}

	skyboxShader->use();
	skyboxShader->setUniform("skybox", 16);
	skyboxShader->setUniform("projection", ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection().getProjectionsMatrix());

	cubemapTex = AssetsModule::TextureHandler::instance()->loadCubemapTexture(skyboxPath)->texture.mId;
	if (cubemapTex == 0) {
		assert(false && "can't load skybox texture");
		return;
	}

	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	VAO.generate();
	VBO.generate();
	VAO.bind();
	VBO.bind();
	VBO.allocateData(108, skyboxVertices);

	VAO.addAttribute(0, 3, GLW::AttributeFType::FLOAT, false, 3 * sizeof(float));
}

void Skybox::draw() {
	if (VAO.getID() == 0) {
		return;
	}
	skyboxShader->use();

	auto view = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getViewMatrix();;
	skyboxShader->setUniform("view", Math::Mat4(Math::Mat3{view}));
	GLW::DepthFuncStack::push(GLW::DepthFunc::LEQUAL);

	GLW::bindTextureToSlot(16, GLW::TEXTURE_CUBE_MAP, cubemapTex);

	VAO.bind();
	GLW::drawVertices(GLW::TRIANGLES, VAO.getID(), 36);
	GLW::DepthFuncStack::pop();
	VAO.bindDefault();
}
