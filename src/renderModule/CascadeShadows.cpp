#include "CascadeShadows.h"

#include <mat4x4.hpp>
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>

#include "imgui.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "ecsModule/EntityManager.h"
#include "ecsModule/SystemManager.h"
#include "glad/glad.h"
#include "logsModule/logger.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "systemsModule/CameraSystem.h"


CascadeShadows::CascadeShadows(size_t entID, glm::vec2 resolution) : Entity(entID) {
	addComponent<TransformComponent>();
	addComponent<LightSourceComponent>(Engine::ComponentsModule::eLightType::DIRECTIONAL);
	auto cmp = addComponent<CascadeShadowComponent>();
	cmp->resolution = resolution;

	setNodeId("cascadeShadows");

	auto& cameraProjection = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<CameraComponent>()->getProjection();

	cmp->shadowCascadeLevels = { cameraProjection.getNear(), 50.f, 150.f, 500.f, cameraProjection.getFar() };
}

const std::vector<glm::mat4>& CascadeShadows::getLightSpaceMatrices() {
	if (!mLightMatricesCache.empty()) {
		return mLightMatricesCache;
	}

	return getComponent<CascadeShadowComponent>()->getLightSpaceMatrices();
}

const std::vector<glm::mat4>& CascadeShadows::getCacheLightSpaceMatrices() {
	return mLightMatricesCache;
}

void CascadeShadows::cacheMatrices() {
	mLightMatricesCache.clear();
	mLightMatricesCache = getLightSpaceMatrices();
}

void CascadeShadows::clearCacheMatrices() {
	mLightMatricesCache.clear();
}

void CascadeShadows::debugDraw(const std::vector<glm::mat4>& lightSpaceMatrices, const glm::mat4& cameraProjection, const glm::mat4& cameraView) {
	if (lightSpaceMatrices.empty()) {
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	auto debugCascadeShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugCascadeShader.vs", "shaders/debugCascadeShader.fs");
	debugCascadeShader->use();
	debugCascadeShader->setMat4("projection", cameraProjection);
	debugCascadeShader->setMat4("view", cameraView);
	drawCascadeVolumeVisualizers(lightSpaceMatrices, debugCascadeShader);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void CascadeShadows::drawCascadeVolumeVisualizers(const std::vector<glm::mat4>& lightMatrices, Engine::ShaderModule::ShaderBase* shader) {
	static std::vector<unsigned> visualizerVAOs;
	static std::vector<unsigned> visualizerVBOs;
	static std::vector<unsigned> visualizerEBOs;

	visualizerVAOs.resize(8);
	visualizerEBOs.resize(8);
	visualizerVBOs.resize(8);

	static const GLuint indices[] = {
		0, 2, 3,
		0, 3, 1,
		4, 6, 2,
		4, 2, 0,
		5, 7, 6,
		5, 6, 4,
		1, 3, 7,
		1, 7, 5,
		6, 7, 3,
		6, 3, 2,
		1, 5, 4,
		0, 1, 4
	};

	static const glm::vec4 colors[] = {
		{1.0, 0.0, 0.0, 0.5f},
		{0.0, 1.0, 0.0, 0.5f},
		{0.0, 0.0, 1.0, 0.5f},
	};

	for (int i = 0; i < lightMatrices.size(); ++i) {
		const auto corners = CascadeShadowComponent::getFrustumCornersWorldSpace(lightMatrices[i]);

		glGenVertexArrays(1, &visualizerVAOs[i]);
		glGenBuffers(1, &visualizerVBOs[i]);
		glGenBuffers(1, &visualizerEBOs[i]);

		glBindVertexArray(visualizerVAOs[i]);

		glBindBuffer(GL_ARRAY_BUFFER, visualizerVBOs[i]);
		glBufferData(GL_ARRAY_BUFFER, corners.size() * sizeof(corners[0]), &corners[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, visualizerEBOs[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);

		glBindVertexArray(visualizerVAOs[i]);
		shader->setVec4("color", colors[i % 3]);
		glDrawElements(GL_TRIANGLES, GLsizei(36), GL_UNSIGNED_INT, 0);

		glDeleteBuffers(1, &visualizerVBOs[i]);
		glDeleteBuffers(1, &visualizerEBOs[i]);
		glDeleteVertexArrays(1, &visualizerVAOs[i]);

		glBindVertexArray(0);
	}

	visualizerVAOs.clear();
	visualizerEBOs.clear();
	visualizerVBOs.clear();
}