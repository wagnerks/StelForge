#include "Renderer.h"

#include <deque>
#include <iostream>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

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

constexpr int glfw_context_ver_maj = 3;
constexpr int glfw_context_ver_min = 3;



using namespace GameEngine;
using namespace GameEngine::RenderModule;
using namespace GameEngine::CoreModule;

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	Renderer::SCR_WIDTH = width;
	Renderer::SCR_HEIGHT = height;
}

void Renderer::draw() {
	RenderModule::Renderer::drawCallsCount = 0;
	RenderModule::Renderer::drawVerticesCount = 0;

	scene->updateScene(0.f);

	//std::vector<glm::mat4> modelMatrices;
	//for (auto backpack : sceneNode->getElement("backpackHolder")->getAllNodes()) {
	//	backpack->getComponent<TransformComponent>()->reloadTransform();
	//	modelMatrices.push_back(backpack->getComponent<TransformComponent>()->getTransform());
	//}

	//auto amount = sceneNode->getElement("backpackHolder")->getAllNodes().size();
	//// vertex buffer object
	//glBindBuffer(GL_ARRAY_BUFFER, buffer);
	//glBufferSubData(GL_ARRAY_BUFFER, 0, amount * sizeof(glm::mat4), &modelMatrices[0]);
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	//glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);
	//glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	//glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));

	//glm::mat4 view = Engine::getInstance()->getCamera()->GetViewMatrix();	       
	//glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
	//glBindBuffer(GL_UNIFORM_BUFFER, 0);

	//glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);


	//auto backpacksCount = sceneNode->getElement("backpackHolder")->getAllNodes().size();

	

	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	

	std::vector<glm::vec3> lights;
	float ampl = 3.f;
	for (unsigned int i = 0; i < lightPositions.size(); i++) {
		auto randSp = randomLightSpeeds[i];
		auto& pos = lightPositions[i];
		lights.push_back(glm::vec3(pos.x + randSp.x * glm::sin(randSp.x * glfwGetTime()), pos.y + randSp.y * glm::cos( randSp.x * glfwGetTime()), pos.z + randSp.z * glm::cos( randSp.x * glfwGetTime())));
	}

	auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
    auto shaderLightingPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");
    auto shaderLightBox = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_light_box.vs", "shaders/deferred_light_box.fs");


    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto camera = Engine::getInstance()->getCamera();
	auto& projection = camera->getProjectionsMatrix();
    auto view = camera->GetViewMatrix();
	glm::mat4 model = glm::mat4(1.0f);

	light->preDraw();
	auto simpleDepth = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/depth.vs", "shaders/depth.fs");
    simpleDepth->use();
    for (unsigned int i = 0; i < objectPositions.size(); i++)
    {
        model = glm::mat4(1.0f);
        model = glm::translate(model, objectPositions[i]);
        model = glm::scale(model, glm::vec3(0.005f));
        //model = glm::scale(model, glm::vec3(1.f));
        simpleDepth->setMat4("model", model);
        modelObj->draw(simpleDepth, true);
		//Utils::renderCube();
    }
	model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.f,-10.f,0.f));
    model = glm::scale(model, glm::vec3(5.f));
	simpleDepth->setMat4("model", model);
	Utils::renderCube();
	light->postDraw();

	light2->preDraw();
    simpleDepth->use();
    for (unsigned int i = 0; i < objectPositions.size(); i++)
    {
        model = glm::mat4(1.0f);
        model = glm::translate(model, objectPositions[i]);
        model = glm::scale(model, glm::vec3(0.005f));
        //model = glm::scale(model, glm::vec3(1.f));
        simpleDepth->setMat4("model", model);
        modelObj->draw(simpleDepth, true);
		//Utils::renderCube();
    }
	model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.f,-10.f,0.f));
    model = glm::scale(model, glm::vec3(5.f));
	simpleDepth->setMat4("model", model);
	Utils::renderCube();
	light2->postDraw();

    // 1. geometry pass: render scene's geometry/color data into gbuffer
    // -----------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderGeometryPass->use();
    shaderGeometryPass->setMat4("projection", projection);
    shaderGeometryPass->setMat4("view", view);
    for (unsigned int i = 0; i < objectPositions.size(); i++)
    {
        model = glm::mat4(1.0f);
        model = glm::translate(model, objectPositions[i]);
        model = glm::scale(model, glm::vec3(0.005f));
        //model = glm::scale(model, glm::vec3(1.f));
        shaderGeometryPass->setMat4("model", model);
        modelObj->draw(shaderGeometryPass);
		//Utils::renderCube();
    }
	model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.f,-10.f,0.f));
    model = glm::scale(model, glm::vec3(5.f));
	shaderGeometryPass->setMat4("model", model);
	Utils::renderCube();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content.
    // -----------------------------------------------------------------------------------------------------------------------
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderLightingPass->use();
	shaderLightingPass->setMat4("DirLights[0].PV", light->getLightSpaceProjection());
	shaderLightingPass->setMat4("DirLights[1].PV", light2->getLightSpaceProjection());
	shaderLightingPass->setInt("DirLights[0].shadowsMap", 3);
	shaderLightingPass->setInt("DirLights[1].shadowsMap", 4);
	shaderLightingPass->setInt("shadowsCount", 2);

	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gPosition);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, gNormal);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, gAlbedoSpec);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE3, GL_TEXTURE_2D, light->getDepthMapTexture());
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE4, GL_TEXTURE_2D, light2->getDepthMapTexture());

	for (unsigned int i = 0; i < lights.size(); i++) {
		shaderLightingPass->setVec3(("lights[" + std::to_string(i) + "].Position").c_str(), lights[i]);
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
		float radius = (-linear + std::sqrt(
			linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
		shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Radius").c_str(), radius);
	}
    shaderLightingPass->setVec3("viewPos", camera->Position);
    // finally render quad
    Utils::renderQuad();
	
    // 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
    // ----------------------------------------------------------------------------------
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
    // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
    // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the 		
    // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
    glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 3. render lights on top of scene
    // --------------------------------
    shaderLightBox->use();
    shaderLightBox->setMat4("projection", projection);
    shaderLightBox->setMat4("view", view);


	for (unsigned int i = 0; i < lights.size(); i++) {
		model = glm::mat4(1.0f);
		model = glm::translate(model, lights[i]);
		model = glm::scale(model, glm::vec3(0.125f));
		shaderLightBox->setMat4("model", model);
		shaderLightBox->setVec3("lightColor", lightColors[i]);
		Utils::renderCube();
	}

	

	auto drawImageShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/imageDraw.vs", "shaders/imageDraw.fs");
	drawImageShader->use();
	drawImageShader->setInt("tex", 0);
	Utils::renderQuad(0.7f, 0.7f,1.f,1.f);
	drawImageShader->setInt("tex", 1);
	Utils::renderQuad(0.7f, 0.4f,1.f,0.7f);
	drawImageShader->setInt("tex", 2);
	Utils::renderQuad(0.7f, 0.1f,1.0f,0.4f);

	
	auto depthDebugShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugDepth.vs", "shaders/debugDepth.fs");
	depthDebugShader->use();
	depthDebugShader->setInt("depthMap", 3);
	depthDebugShader->setFloat("near_plane", light->getNearPlane());
	depthDebugShader->setFloat("far_plane", light->getFarPlane());
	Utils::renderQuad(0.4f, 0.1f,0.7f,0.4f);

	TextureHandler::getInstance()->bindTexture(GL_TEXTURE6, GL_TEXTURE_2D, light2->getDepthMapTexture());
	depthDebugShader->use();
	depthDebugShader->setInt("depthMap", 6);
	depthDebugShader->setFloat("near_plane", light2->getNearPlane());
	depthDebugShader->setFloat("far_plane", light2->getFarPlane());
	Utils::renderQuad(0.1f, 0.1f,0.4f,0.4f);

	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gPosition);
	auto drawImageShaderDT = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/imageDrawDepthTransform.vs", "shaders/imageDrawDepthTransform.fs");
	drawImageShaderDT->use();
	drawImageShaderDT->setInt("tex", 3);
	drawImageShaderDT->setInt("uPos", 0);
	drawImageShaderDT->setMat4("lightSpaceMatrix", light->getLightSpaceProjection());
	Utils::renderQuad(0.4f, -.2f,0.7f,0.1f);


	drawImageShaderDT->use();
	drawImageShaderDT->setInt("tex", 6);
	drawImageShaderDT->setInt("uPos", 0);
	drawImageShaderDT->setMat4("lightSpaceMatrix", light2->getLightSpaceProjection());
	Utils::renderQuad(0.1f, -.2f,0.4f,0.1f);

	auto mergeShadows = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/mergeDepths.vs", "shaders/mergeDepths.fs");
	mergeShadows->use();
	mergeShadows->setMat4("lights[0].PV", light->getLightSpaceProjection());
	mergeShadows->setMat4("lights[1].PV", light2->getLightSpaceProjection());
	mergeShadows->setInt("lights[0].shadowsMap", 3);
	mergeShadows->setInt("shadowsCount", 2);

	mergeShadows->setInt("lights[1].shadowsMap", 6);

	mergeShadows->setInt("uPos", 0);

	Utils::renderQuad(0.7f, -0.2f,1.f,0.1f);

	int  i =0;
	for (auto light : lightsObj) {

		Debug::ComponentsDebug::transformComponentDebug("light" + std::to_string(i), light->getComponent<TransformComponent>());

		auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/light.vs", "shaders/light.fs");
	    shader->use();
		shader->setMat4("projection", projection);
		shader->setMat4("view", view);
		auto scale = light->getComponent<TransformComponent>()->getScale();
		light->getComponent<TransformComponent>()->setScale({0.5f,0.5f,0.5f});
		light->getComponent<TransformComponent>()->reloadTransform();
		shader->setMat4("model", light->getComponent<TransformComponent>()->getTransform());
		GameEngine::RenderModule::Utils::renderCube();
		light->getComponent<TransformComponent>()->setScale(scale);
		light->getComponent<TransformComponent>()->reloadTransform();

		auto lines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/xyzLines.fs");
		lines->use();
		lines->setMat4("PVM",GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix() * GameEngine::Engine::getInstance()->getCamera()->GetViewMatrix() *  light->getComponent<TransformComponent>()->getTransform());
		GameEngine::RenderModule::Utils::renderXYZ(20.f);
		i++;
	}
	

	//scene->drawScene();
}

void Renderer::postDraw() {
	glfwSwapBuffers(Engine::getInstance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	scene = new GameModule::CoreModule::Scene();
	scene->init();

	/*

	auto backpacks = new NodeModule::Node("backpackHolder");
	sceneNode->addElement(backpacks);*/

	/*int n =0;
	auto max = 15;
	float dif = 3.f;
	for (auto i = 0; i < max; i++) {
		for (auto j = 0; j < max; j++) {
			for (auto k = 0; k < max; k++) {
				auto backpack = new NodeModule::Node("backpack" + std::to_string(n));
				backpack->getComponent<ModelComponent>()->setModel(modelObj);
				auto tc = backpack->getComponent<TransformComponent>();
				tc->setPos({i * dif, j * dif, k * -dif});
				tc->setRotateX(-90.f);
				backpacks->addElement(backpack);
				n++;
			}
		}
	}*/
	//std::vector<glm::mat4> modelMatrices;
	//for (auto backpack : backpacks->getAllNodes()) {
	//	backpack->getComponent<TransformComponent>()->reloadTransform();
	//	modelMatrices.push_back(backpack->getComponent<TransformComponent>()->getTransform());
	//}

	//auto amount = backpacks->getAllNodes().size();
	//// vertex buffer object
	//
	//glGenBuffers(1, &buffer);
	//glBindBuffer(GL_ARRAY_BUFFER, buffer);
	//glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), modelMatrices.data(), GL_STATIC_DRAW);
	//  
	//for(unsigned int i = 0; i < modelObj->getMeshes().size(); i++)
	//{
	//    unsigned int VAO = modelObj->getMeshes()[i].getVAO();
	//    glBindVertexArray(VAO);
	//	auto vertexOffset = sizeof(ModelModule::Vertex);
	//    // set attribute pointers for matrix (4 times vec4)
 //       glEnableVertexAttribArray(3);
 //       glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
 //       glEnableVertexAttribArray(4);
 //       glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0 + sizeof(glm::vec4)));
 //       glEnableVertexAttribArray(5);
 //       glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0 + 2 * sizeof(glm::vec4)));
 //       glEnableVertexAttribArray(6);
 //       glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0 + 3 * sizeof(glm::vec4)));

 //       glVertexAttribDivisor(3, 1);
 //       glVertexAttribDivisor(4, 1);
 //       glVertexAttribDivisor(5, 1);
 //       glVertexAttribDivisor(6, 1);

	//    glBindVertexArray(0);
	//}  
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	initGlobalProjection();

	// build and compile shaders
    // -------------------------
    auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
    auto shaderLightingPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");
    auto shaderLightBox = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_light_box.vs", "shaders/deferred_light_box.fs");

	modelObj = new ModelModule::Model("model/scene.gltf");

	modelObj->getComponent<TransformComponent>()->setScale({0.03f,0.03f,0.03f});

    objectPositions.push_back(glm::vec3(-3.0,  3.5, -3.0));
    objectPositions.push_back(glm::vec3( 0.0,  3.5, -3.0));
    objectPositions.push_back(glm::vec3( 3.0,  3.5, -3.0));
    objectPositions.push_back(glm::vec3(-3.0,  3.5,  0.0));
    objectPositions.push_back(glm::vec3( 0.0,  3.5,  0.0));
    objectPositions.push_back(glm::vec3( 3.0,  3.5,  0.0));
    objectPositions.push_back(glm::vec3(-3.0,  3.5,  3.0));
    objectPositions.push_back(glm::vec3( 0.0,  3.5,  3.0));
    objectPositions.push_back(glm::vec3( 3.0,  3.5,  3.0));

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
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

    glDrawBuffers(3, attachments);
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// lighting info
    // -------------
    const unsigned int NR_LIGHTS = 10;
    
    srand(13);
    for (unsigned int i = 0; i < NR_LIGHTS; i++)
    {
        // calculate slightly random offsets
        float xPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        float yPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 4.0);
        float zPos = static_cast<float>(((rand() % 100) / 100.0) * 6.0 - 3.0);
        lightPositions.push_back(glm::vec3(xPos, yPos, zPos));
        // also calculate random color
        float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        lightColors.push_back(glm::vec3(rColor, gColor, bColor));

		randomLightSpeeds.push_back(glm::vec3(glm::linearRand(2.f,5.f), glm::linearRand(2.f,4.f), glm::linearRand(2.f,10.f)));
    }
	
	// shader configuration
    // --------------------
    shaderLightingPass->use();
    shaderLightingPass->setInt("gPosition", 0);
    shaderLightingPass->setInt("gNormal", 1);
    shaderLightingPass->setInt("gAlbedoSpec", 2);

	light = new LightsModule::DirectionalLight();
	light->getComponent<TransformComponent>()->setScale({5.f,5.f,1.f});
	light->getComponent<TransformComponent>()->setY(20.f);
	light->getComponent<TransformComponent>()->setRotate({90.f,0.f,0.f});

	light2 = new LightsModule::DirectionalLight();
	light2->getComponent<TransformComponent>()->setScale({5.f,5.f,1.f});
	light2->getComponent<TransformComponent>()->setX(2.f);
	light2->getComponent<TransformComponent>()->setZ(2.f);
	light2->getComponent<TransformComponent>()->setY(20.f);
	light2->getComponent<TransformComponent>()->setRotate({105.f,0.f,0.f});

	lightsObj.push_back(light);
	lightsObj.push_back(light2);


}

void Renderer::terminate() const {
	glfwTerminate();
}

void Renderer::drawCall() {
	
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
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glfw_context_ver_maj);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glfw_context_ver_min);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	auto window = glfwCreateWindow(Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr) {
		LogsModule::Logger::LOG_ERROR("Failed to create GLFW window");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(0);


	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);

	glClearColor(0.f, 0.f, 0.f, 1.0f);
	//glClearColor(0.07f, 0.13f, 0.17f, 1.0f);


	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
