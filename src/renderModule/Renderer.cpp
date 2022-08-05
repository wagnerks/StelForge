#include "Renderer.h"

#include <deque>
#include <functional>
#include <iostream>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "imgui.h"
#include "TextureHandler.h"
#include "Utils.h"
#include "componentsModule/DrawComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "debugModule/ComponentsDebug.h"
#include "logsModule/logger.h"
#include "modelModule/MeshFactory.h"
#include "modelModule/Model.h"
#include "nodeModule/Node.h"
#include "shaderModule/ShaderController.h"
#include "gtc/random.hpp"
#include "core/BoundingVolume.h"
#include "core/ModelLoader.h"
#include "ecsModule/EntityManager.h"

constexpr int GLFW_CONTEXT_VER_MAJ = 4;
constexpr int GLFW_CONTEXT_VER_MIN = 6;


using namespace GameEngine;
using namespace GameEngine::RenderModule;
using namespace GameEngine::CoreModule;


void Renderer::draw() {
	RenderModule::Renderer::drawCallsCount = 0;
	RenderModule::Renderer::drawVerticesCount = 0;

	scene->updateScene(0.f);
	
	ImGui::Begin("comp test");
	if (ImGui::Button("add")) {
		node->getComponent<TransformComponent>();
	}
	if (ImGui::Button("remove")) {
		node->removeComponent<TransformComponent>();
	}
	if (ImGui::Button("removeall")) {
		ecsModule::ECSHandler::componentManagerInstance()->removeAllComponents(node->getEntityID());
		node->removeComponent<TransformComponent>();
	}
	ImGui::End();

	auto camera = Engine::getInstance()->getCamera();
	ImGui::Begin("keklol");
	ImGui::Checkbox("update cam frustum", &updateFrustum);
	ImGui::End();

	if (updateFrustum) {
		camFrustum = FrustumModule::createPerspectiveProjectionFrustum(camera->getComponent<TransformComponent>(), camera->cameraView.getAspect(), glm::radians(camera->cameraView.getFOV()), camera->cameraView.getZNear(), camera->cameraView.getZFar());
	}


	auto& projection = camera->getProjectionsMatrix();
    auto view = camera->getComponent<TransformComponent>()->getViewMatrix();
	Utils::initCubeVAO();
	lightPositions.clear();

	auto conta = ecsModule::ECSHandler::componentManagerInstance()->getComponentContainer<TransformComponent>();

	for (auto& transform : *conta) {
		transform.reloadTransform();
		Debug::ComponentsDebug::transformComponentDebug(std::to_string(transform.getOwnerId()), &transform);
		if (auto modelComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(transform.getOwnerId())->getComponent<ModelComponent>(false)){
			if (modelComp->getModel()) {
				for (auto& mesh : modelComp->getModel()->getMeshes()) {
					unsigned tex = -1;
					//auto sphere = FrustumModule::generateSphereBV(mesh);
					if (mesh->bounds->isOnFrustum(camFrustum, transform)) {
						batcher->addToDrawList(mesh->getVAO(), mesh->vertices.size(), mesh->indices.size(),mesh->textures, transform.getTransform(), false);
					}

				}
			}
			
		}
		else {
			batcher->addToDrawList(Utils::cubeVAO, 36, 0,{}, transform.getTransform(), false);
		}
	}


	/*for (auto& transform : TransformComponent::components) {
		transform.reloadTransform();
		Debug::ComponentsDebug::transformComponentDebug(std::to_string(transform.getOwner()->getID()), &transform);
		if (auto modelComp = transform.getOwner()->getComponent<ModelComponent>(false)){
			if (modelComp->getModel()) {
				for (auto& mesh : modelComp->getModel()->getMeshes()) {
					unsigned tex = -1;
					auto sphere = FrustumModule::generateSphereBV(mesh);
					if (mesh->bounds->isOnFrustum(camFrustum, transform)) {
						batcher->addToDrawList(mesh->getVAO(), mesh->vertices.size(), mesh->indices.size(),mesh->textures, transform.getTransform(), false);
					}

				}
			}
			
		}
		else {
			batcher->addToDrawList(Utils::cubeVAO, 36, 0,{}, transform.getTransform(), false);
		}
	}*/

	//for (auto element : node->getElements()) {
	//	float koef = 0.01f;
	//	auto transform = element->getComponent<TransformComponent>();
	//	if (false && element->getId().find("cube") == std::string::npos) {
	//		transform->setPos(glm::vec3(transform->getPos().x, transform->getPos().y + glm::linearRand(-5.f*koef,5.f*koef), transform->getPos().z));
	//		transform->setRotate(glm::vec3(transform->getRotate().x + 3.f, transform->getRotate().y + 3.f, transform->getRotate().z));
	//		transform->setScale(glm::vec3(transform->getScale().x + glm::linearRand(-0.1f*koef,0.1f*koef), transform->getScale().y + glm::linearRand(-0.1f*koef,0.1f*koef), transform->getScale().z + glm::linearRand(-0.1f*koef,0.1f*koef)));

	//		//lightPositions.push_back(transform->getPos());
	//		
	//	}
	//	if (auto modelComp = element->getComponent<ModelComponent>(false)){
	//		if (modelComp->getModel()) {
	//			for (auto& mesh : modelComp->getModel()->getMeshes()) {
	//				unsigned tex = -1;
	//				//auto sphere = FrustumModule::generateSphereBV(mesh);
	//				if (mesh->bounds->isOnFrustum(camFrustum, *element->getComponent<TransformComponent>())) {
	//					batcher->addToDrawList(mesh->getVAO(), mesh->vertices.size(), mesh->indices.size(),mesh->textures, transform->getTransform(), false);
	//				}

	//			}
	//		}
	//		
	//	}
	//	else {
	//		if (element->getId() == "cube") {
	//			batcher->addToDrawList(Utils::cubeVAO, 36, 0,{}, transform->getTransform(), true);
	//		}
	//		else {
	//			batcher->addToDrawList(Utils::cubeVAO, 36, 0,{}, transform->getTransform(), false);
	//		}
	//	}
	//}


	for (auto dirLight : lightsObj) {
		dirLight->preDraw();
		batcher->flushAll(false, dirLight->getComponent<TransformComponent>()->getPos(), true);
		dirLight->postDraw();
	}


	if (cascade) {
		cascade->preDraw();
		batcher->flushAll(false, cascade->getLightPosition(), true);
		cascade->postDraw();
	}


	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    // 1. geometry pass: render scene's geometry/color data into gbuffer
    // -----------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
    shaderGeometryPass->use();
	shaderGeometryPass->setMat4("PV", projection * view);
	shaderGeometryPass->setInt("texture_diffuse1" , 0);
	shaderGeometryPass->setInt("normalMap" , 1);
	batcher->flushAll(true);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content.
    // -----------------------------------------------------------------------------------------------------------------------
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto shaderLightingPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");

    shaderLightingPass->use();
    shaderLightingPass->setInt("gPosition", 0);
    shaderLightingPass->setInt("gNormal", 1);
    shaderLightingPass->setInt("gAlbedoSpec", 2);

	if (cascade) {
		shaderLightingPass->setFloat("bias", cascade->getBias());
		TextureHandler::getInstance()->bindTexture(GL_TEXTURE31, GL_TEXTURE_2D_ARRAY, cascade->getShadowMapTextureArray());
		shaderLightingPass->setInt("cascadedShadow.shadowMap", 31);
		shaderLightingPass->setVec3("cascadedShadow.direction", cascade->getLightDirection());
		shaderLightingPass->setVec3("cascadedShadow.color", cascade->getLightColor());
		shaderLightingPass->setVec2("cascadedShadow.texelSize", 1.f / cascade->getResolution());
		shaderLightingPass->setInt("cascadeCount", static_cast<int>(cascade->getShadowCascadeLevels().size()));
	    for (size_t i = 0; i < cascade->getShadowCascadeLevels().size(); ++i) {
	        shaderLightingPass->setFloat(("cascadePlaneDistances[" + std::to_string(i) + "]").c_str(), cascade->getShadowCascadeLevels()[i]);
	    }
		shaderLightingPass->setFloat("farPlane", cascade->getCameraFarPlane());
	}


    shaderLightingPass->setMat4("view", view);
    // set light uniforms
	shaderLightingPass->setVec3("viewPos", camera->getComponent<TransformComponent>()->getPos());


	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gPosition);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, gNormal);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, gAlbedoSpec);

	shaderLightingPass->setInt("shadowsCount", static_cast<int>(lightsObj.size()));
	for (auto i = 0u; i < lightsObj.size(); i++) {
		auto lightSource = lightsObj[i];
		shaderLightingPass->setMat4(("DirLights[" + std::to_string(i) + "].PV").c_str(), lightSource->getProjectionViewMatrix());
		shaderLightingPass->setVec3(("DirLights[" + std::to_string(i) + "].Position").c_str(), lightSource->getComponent<TransformComponent>()->getPos());
		shaderLightingPass->setInt(("DirLights[" + std::to_string(i) + "].shadowsMap").c_str(), i + 3);
		TextureHandler::getInstance()->bindTexture(GL_TEXTURE3 + i, GL_TEXTURE_2D, lightSource->getDepthMapTexture());
	}

	
	shaderLightingPass->setInt("lightsCount", static_cast<int>(lightPositions.size()));

	for (unsigned int i = 0; i < lightPositions.size(); i++) {
		shaderLightingPass->setVec3(("lights[" + std::to_string(i) + "].Position").c_str(), lightPositions[i]);
		shaderLightingPass->setVec3(("lights[" + std::to_string(i) + "].Color").c_str(), lightColors[i]);
		// update attenuation parameters and calculate radius
		const float constant = 1.0f;
		// note that we don't send this to the shader, we assume it is always 1.0 (in our case)
		const float linear = 0.7f;
		const float quadratic = 1.8f;
		shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Linear").c_str(), linear);
		shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Quadratic").c_str(), quadratic);
		// then calculate radius of light volume/sphere
		const float maxBrightness = std::fmaxf(std::fmaxf(lightColors[i].r, lightColors[i].g), lightColors[i].b);
		float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
		shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Radius").c_str(), radius);
	}
    
    // finally render quad
    Utils::renderQuad();
	
    // 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
    // ----------------------------------------------------------------------------------
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
    glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 3. render lights on top of scene
    // --------------------------------
    auto shaderLightBox = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_light_box.vs", "shaders/deferred_light_box.fs");

    shaderLightBox->use();
    shaderLightBox->setMat4("projection", projection);
    shaderLightBox->setMat4("view", view);

	for (unsigned int i = 0; i < lightPositions.size(); i++) {
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, lightPositions[i]);
		model = glm::scale(model, glm::vec3(0.125f));
		shaderLightBox->setMat4("model", model);
		shaderLightBox->setVec3("lightColor", lightColors[i]);
		batcher->addToDrawList(Utils::cubeVAO, 36, 0, {}, model, false);
	}
	batcher->flushAll(true);

	int  i =0;

	for (auto light : lightsObj) {

		Debug::ComponentsDebug::transformComponentDebug("light" + std::to_string(i), light->getComponent<TransformComponent>());

		auto scale = light->getComponent<TransformComponent>()->getScale();
		light->getComponent<TransformComponent>()->setScale({0.5f,0.5f,0.5f});
		light->getComponent<TransformComponent>()->reloadTransform();
		
		batcher->addToDrawList(Utils::cubeVAO, 36, 0, {}, light->getComponent<TransformComponent>()->getTransform(), false);
		light->getComponent<TransformComponent>()->setScale(scale);
		light->getComponent<TransformComponent>()->reloadTransform();

		auto lines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/xyzLines.fs");
		lines->use();
		lines->setMat4("PVM",GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix() * GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getViewMatrix() *  light->getComponent<TransformComponent>()->getTransform());
		GameEngine::RenderModule::Utils::renderXYZ(20.f);
		i++;
	}
	
	auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/light.vs", "shaders/light.fs");
    shader->use();
	shader->setMat4("PV", projection * view);
	if (cascade) {
		auto pos = cascade->getLightPosition();
		glm::mat4 model = glm::translate(glm::mat4(1.0f), pos) * glm::mat4(1.f) * glm::scale(glm::mat4(1.0f), {3.f,3.f,3.f});
		batcher->addToDrawList(Utils::cubeVAO, 36, 0, {}, model, false);
	}

	batcher->flushAll(true);

	if (ImGui::Begin("lightSpaceMatrix")) {
		ImGui::DragFloat("camera speed", &Engine::getInstance()->getCamera()->MovementSpeed, 0.1f);

		if (cascade) {
			if(ImGui::Button("cache")) {
				cascade->cacheMatrices();
			}
			if(ImGui::Button("clear")) {
				cascade->clearCacheMatrices();
			}
			
			float color[3] = {cascade->getLightColor().x, cascade->getLightColor().y, cascade->getLightColor().z};
			if (ImGui::ColorEdit3("sunColor", color)) {
				cascade->setLightColor({color[0], color[1], color[2]});
			}
			float lightPosDir[] = {cascade->getLightPosition().x, cascade->getLightPosition().y, cascade->getLightPosition().z};
			if (ImGui::DragFloat3("sun position",lightPosDir , 0.01f)) {
				cascade->setLightPosition({lightPosDir[0], lightPosDir[1], lightPosDir[2]});
			}
			float cascadeBias = cascade->getBias();
			if (ImGui::DragFloat("bias", &cascadeBias, 0.00001f,0.f,1.f, "%.5f")) {
				cascade->setBias(cascadeBias);
			}

			if (ImGui::DragFloat("sun pos", &cascade->sunProgress, 0.001f, 0.f)) {
				auto x = glm::cos(glm::radians(-cascade->sunProgress * 180.f));
				auto y = glm::sin(glm::radians(cascade->sunProgress * 180.f));
				auto z = glm::sin(glm::radians(cascade->sunProgress * 180.f));
				cascade->setLightPosition({x * 80.f, y * 30.f, z * 10.f + 0.001f});
			}
		}
	}
	ImGui::End();

	if (cascade) {
		//cascade->sunProgress += 0.001f;
		/*auto x = glm::cos(glm::radians(-cascade->sunProgress * 180.f));
		auto y = glm::sin(glm::radians(cascade->sunProgress * 180.f));
		auto z = glm::sin(glm::radians(cascade->sunProgress * 180.f));
		cascade->setLightPosition({x * 80.f, y * 30.f, z * 10.f + 0.001f});*/
	}
	
	//cascade->setLightColor(glm::vec3(1.f, std::max(std::min(1.f, y* 2.f), 0.f), std::max(std::min(1.f, y* 3.f), 0.f)));
	//scene->drawScene();
	if (cascade) {
		cascade->debugDraw();
	}

	ImGui::Begin("kek");
	float size = 500.f;
	ImGui::Image((ImTextureID)gAlbedoSpec, {size,size}, {0.f, 1.f}, {1.f,0.f});
	ImGui::Image((ImTextureID)gPosition, {size,size}, {0.f, 1.f}, {1.f,0.f});
	ImGui::Image((ImTextureID)gNormal, {size,size}, {0.f, 1.f}, {1.f,0.f});
	ImGui::Image((ImTextureID)gDepthBuffer, {size,size}, {0.f, 1.f}, {1.f,0.f});

	ImGui::End();
}

void Renderer::postDraw() {
	glfwSwapBuffers(Engine::getInstance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	scene = new GameModule::CoreModule::Scene();
	scene->init();

	initGlobalProjection();

	// build and compile shaders
    // -------------------------

	//modelObj = ModelLoader::getInstance()->load("model/scene.gltf");
	//objectPositions.emplace_back(glm::vec3(glm::linearRand(2.3f,4.f), glm::linearRand(2.3f,4.f), glm::linearRand(2.3f,4.f)));

	modelObj = ModelLoader::getInstance()->load("suzanne/scene.gltf");
	
	auto count = 20;
	for (auto i = -count; i < count; i++) {
		for (auto j = -count; j < count; j++) {
			for (auto k = 1u; k < count / 2; k++) {
				objectPositions.emplace_back(glm::vec3(i * glm::linearRand(2.3f,4.f), k * glm::linearRand(2.3f,4.f), j * glm::linearRand(2.3f,4.f)));
			}
			
		}
	}
	auto id = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("lel");

	node = static_cast<NodeModule::Node*>(ecsModule::ECSHandler::entityManagerInstance()->getEntity(id));
	for (auto i = 0u; i < objectPositions.size(); i++) {
		auto objectPos = objectPositions[i];
		auto id = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("backpack" + std::to_string(i));

		auto backpack = static_cast<NodeModule::Node*>(ecsModule::ECSHandler::entityManagerInstance()->getEntity(id));
		backpack->getComponent<TransformComponent>()->setPos(objectPos);
		//backpack->getComponent<TransformComponent>()->setScale({0.05f,0.05f,0.05f});
		backpack->getComponent<TransformComponent>()->setRotate({-90.f,0.f,0.f});
		backpack->getComponent<ModelComponent>()->setModel(modelObj);
		node->addElement(backpack);
	}
	
	id = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("cube");

	auto cube = static_cast<NodeModule::Node*>(ecsModule::ECSHandler::entityManagerInstance()->getEntity(id));

	node->addElement(cube);
	id = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("cube2");

	auto cube2 = static_cast<NodeModule::Node*>(ecsModule::ECSHandler::entityManagerInstance()->getEntity(id));
	node->addElement(cube2);
	cube2->getComponent<TransformComponent>()->setScale({1.f,10.f,50.f});
	cube2->getComponent<TransformComponent>()->setPos({-10.f,0.f,0.f});


	cube->getComponent<TransformComponent>()->setScale({50.f,1.f,50.f});
	cube->getComponent<TransformComponent>()->setPos({0.f,-1.f,0.f});


	// configure g-buffer framebuffer
    // ------------------------------
    
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
   
    // position color buffer
    glGenTextures(1, &gPosition);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gPosition);
    //glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    // normal color buffer
    glGenTextures(1, &gNormal);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gNormal);
    //glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    // color + specular color buffer
    glGenTextures(1, &gAlbedoSpec);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gAlbedoSpec);
    //glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering

	glGenTextures(1, &gDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, gDepthBuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, static_cast<GLsizei>(SCR_WIDTH), static_cast<GLsizei>(SCR_HEIGHT), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepthBuffer, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);


    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
	glDrawBuffers(3, attachments);

    // create and attach depth buffer (renderbuffer)
 /*   glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);*/
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// lighting info
    // -------------
    const unsigned int NR_LIGHTS = 0;
    
    srand(13);
	for (unsigned int i = 0; i < 1600; i++) {
		// calculate slightly random offsets

		// also calculate random color
		float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
		float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
		float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
		lightColors.push_back(glm::vec3(rColor, gColor, bColor));
	}
	
	// shader configuration
    // --------------------


	cascade = new CascadeShadows({2048, 2048});


	batcher = new Batcher();

	//lightsObj.push_back(new LightsModule::DirectionalLight(1024,1024));
}

void Renderer::terminate() const {
	glfwTerminate();
}

void Renderer::drawCall() {
	
}

void Renderer::drawArrays(GLenum mode, GLsizei size, GLint first) {
	glDrawArrays(mode, first, size);
	drawCallsCount++;
	drawVerticesCount += size;
}

void Renderer::drawElements(GLenum mode, GLsizei size, GLenum type, const void* place) {
	glDrawElements(mode, size, type, place);
	drawCallsCount++;
	drawVerticesCount += size;
}

void Renderer::drawElementsInstanced(GLenum mode, GLsizei size, GLenum type, GLsizei instancesCount, const void* place) {
	glDrawElementsInstanced(mode, size, type, place, instancesCount);
	drawCallsCount++;
	drawVerticesCount += size * instancesCount;
}

void Renderer::drawArraysInstancing(GLenum mode, GLsizei size, GLsizei instancesCount, GLint first) {
	glDrawArraysInstanced(mode, first, size, instancesCount);
	drawCallsCount++;
	drawVerticesCount += size * instancesCount;
}

void Renderer::initGlobalProjection() {
	//todo
	/*
	unsigned int main    = glGetUniformBlockIndex(SHADER_CONTROLLER->loadVertexFragmentShader("shaders/main.vs", "shaders/main.fs")->getID(), "Matrices");
	
	auto geom = SHADER_CONTROLLER->loadGeometryShader("shaders/geometry.vs","shaders/geometry.fs","shaders/geometry.gs");
	unsigned int getomPlace    = glGetUniformBlockIndex(geom->getID(), "Matrices");

	glUniformBlockBinding(SHADER_CONTROLLER->loadVertexFragmentShader("shaders/main.vs", "shaders/main.fs")->getID(),    main, 0);
	glUniformBlockBinding(geom->getID(), getomPlace, 0);

	
	glGenBuffers(1, &uboMatrices);
	  
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	  
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));

	
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glm::mat4 view = Engine::getInstance()->getCamera()->GetViewMatrix();	       
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
	glBindBuffer(GL_UNIFORM_BUFFER, 0); */
}

GLFWwindow* Renderer::initGLFW() {
	if (GLFWInited) {
		assert(false && "GLFW Already inited");
		return nullptr;
	}
	GLFWInited = true;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	auto window = glfwCreateWindow(Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, "GameEngine", nullptr, nullptr);
	if (window == nullptr) {
		LogsModule::Logger::LOG_ERROR("Failed to create GLFW window");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		glViewport(0, 0, width, height);
		Renderer::SCR_WIDTH = width;
		Renderer::SCR_HEIGHT = height;
	});

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(0);


	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	//glDepthFunc(GL_LESS);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);
	glClearDepth(1000.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);


	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
