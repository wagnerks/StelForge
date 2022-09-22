#include "CascadeShadows.h"

#include <mat4x4.hpp>
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/LightComponent.h"
#include "componentsModule/ProjectionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "ecsModule/EntityManager.h"
#include "glad/glad.h"
#include "logsModule/logger.h"
#include "shaderModule/ShaderController.h"

CascadeShadow::CascadeShadow(size_t entID) : Entity(entID) {
	addComponent<TransformComponent>();
	addComponent<FrustumComponent>();
	addComponent<ProjectionComponent>();
	auto light = addComponent<LightComponent>(GameEngine::ComponentsModule::eLightType::DIRECTIONAL);
	light->setBias(0.001f);

	setNodeId("cascade");
}

CascadeShadows::CascadeShadows(size_t entID, glm::vec2 resolution) : Entity(entID), resolution(resolution) {
	addComponent<TransformComponent>();
	setNodeId("cascadeShadows");
}

CascadeShadows::~CascadeShadows() {
	glDeleteFramebuffers(1, &lightFBO);
	glDeleteTextures(1, &lightDepthMaps);
	glDeleteBuffers(1, &matricesUBO);
}

void CascadeShadows::init() {
	auto camera = GameEngine::Engine::getInstance()->getCamera();
	auto cameraProjection = camera->getComponent<ProjectionComponent>();

	auto zNear = cameraProjection->getProjection().getNear();
	auto zFar = cameraProjection->getProjection().getFar();

	auto fov = cameraProjection->getProjection().getFOV();
	auto aspect = cameraProjection->getProjection().getAspect();
	
	shadowCascadeLevels = {zNear, 25.f, 75.f, 150.f, 500.f, zFar};

	for (size_t i = 1; i < shadowCascadeLevels.size(); ++i) {
		shadows.emplace_back(ecsModule::ECSHandler::entityManagerInstance()->createEntity<CascadeShadow>());
		shadows.back()->proj = glm::perspective(fov, aspect, shadowCascadeLevels[i - 1], shadowCascadeLevels[i]); //todo this should be some cascaded system part
		addElement(shadows.back());
	}


	

	glGenFramebuffers(1, &lightFBO);
    
	glGenTextures(1, &lightDepthMaps);
	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
	glTexImage3D(
	    GL_TEXTURE_2D_ARRAY,
	    0,
	    GL_DEPTH_COMPONENT32,
	    static_cast<int>(resolution.x),
	    static_cast<int>(resolution.y),
	    int(shadowCascadeLevels.size()) - 1,
	    0,
	    GL_DEPTH_COMPONENT,
	    GL_FLOAT,
	    nullptr);
    
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
	    
	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		GameEngine::LogsModule::Logger::LOG_ERROR("FRAMEBUFFER::CascadeShadow Framebuffer is not complete!");
	}
	    
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenBuffers(1, &matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void CascadeShadows::preDraw() {
	const auto lightMatrices = getLightSpaceMatrices();
	if (!lightMatrices.empty()){
		glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4x4) * lightMatrices.size(), &lightMatrices[0]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, lightDepthMaps, 0);
	glViewport(0, 0, static_cast<int>(resolution.x), static_cast<int>(resolution.y));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();
}

void CascadeShadows::postDraw() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, GameEngine::RenderModule::Renderer::SCR_WIDTH, GameEngine::RenderModule::Renderer::SCR_HEIGHT);
}

void CascadeShadows::debugDraw() {
	if (!lightMatricesCache.empty()){
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		auto debugCascadeShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugCascadeShader.vs", "shaders/debugCascadeShader.fs");
		debugCascadeShader->use();
		debugCascadeShader->setMat4("projection", GameEngine::Engine::getInstance()->getCamera()->getComponent<ProjectionComponent>()->getProjection().getProjectionsMatrix());
		debugCascadeShader->setMat4("view", GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getViewMatrix());
		drawCascadeVolumeVisualizers(lightMatricesCache, debugCascadeShader);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);
	}
}


std::vector<glm::vec4> CascadeShadow::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
	return getFrustumCornersWorldSpace(proj * view);
}

std::vector<glm::vec4> CascadeShadow::getFrustumCornersWorldSpace(const glm::mat4& projView) {
	const auto inv = glm::inverse(projView);

	std::vector<glm::vec4> frustumCorners;
	for (unsigned int x = 0; x < 2; ++x) {
		for (unsigned int y = 0; y < 2; ++y) {
			for (unsigned int z = 0; z < 2; ++z) {
				const glm::vec4 pt = inv * glm::vec4(
					2.0f * static_cast<float>(x) - 1.0f, 
					2.0f * static_cast<float>(y) - 1.0f, 
					2.0f * static_cast<float>(z) - 1.0f, 
					1.0f
				);

				frustumCorners.push_back(pt / pt.w);
			}
		}
	}

	return frustumCorners;
}

glm::mat4 CascadeShadow::getLightSpaceMatrix(const std::vector<glm::vec4>& corners) {
	glm::vec3 center = glm::vec3(0, 0, 0);
	for (const auto& v : corners) {
		center += glm::vec3(v);
	}

	center /= corners.size();

	getComponent<TransformComponent>()->setPos(center);
	getComponent<TransformComponent>()->reloadTransform();

	const auto lightView = getComponent<TransformComponent>()->getViewMatrix();
	
	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::min();

	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::min();

	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::min();
	
	for (const auto& v : corners) {
		const auto transform = lightView * v;

		minX = std::min(minX, transform.x);
		maxX = std::max(maxX, transform.x);

		minY = std::min(minY, transform.y);
		maxY = std::max(maxY, transform.y);

		minZ = std::min(minZ, transform.z);
		maxZ = std::max(maxZ, transform.z);
	}

	if (minZ < 0) {
		minZ *= mZMult;
	}
	else {
		minZ /= mZMult;
	}

	if (maxZ < 0) {
		maxZ /= mZMult;
	}
	else {
		maxZ *= mZMult;
	}

	auto projComp = getComponent<ProjectionComponent>();
	projComp->initProjection({minX, minY}, {maxX, maxY}, minZ, maxZ);
	

	auto size = std::fabs(std::max((maxX - minX), (maxY - minY)));
	auto lightComp = getComponent<LightComponent>();
	
	lightComp->setTexelSize(glm::vec2(1.f / std::fabs(maxX - minX), 1.f / std::fabs(maxY - minY)) * mTexelsMultiplier);
	//lightComp->setBias(0.001f);//1.f / size * mBiasMultiplier);

	
	return projComp->getProjection().getProjectionsMatrix() * lightView;
}

std::vector<glm::mat4> CascadeShadows::getLightSpaceMatrices() {
	if (!lightMatricesCache.empty()) {
		return lightMatricesCache;
	}

	const auto view = GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getViewMatrix();

	std::vector<glm::mat4> ret;

	for (auto shadowCascade : shadows) {
		const auto corners = shadowCascade->getFrustumCornersWorldSpace(shadowCascade->proj, view);
		auto projViewMatrix = shadowCascade->getLightSpaceMatrix(corners);
		ret.push_back(projViewMatrix);

		shadowCascade->getComponent<FrustumComponent>()->updateFrustum(projViewMatrix);
	}

	return ret;
}

const std::vector<float>& CascadeShadows::getShadowCascadeLevels() {
	return shadowCascadeLevels;
}

const glm::vec3& CascadeShadows::getLightPosition() const {
	return getComponent<TransformComponent>()->getPos(true);
}
glm::vec3 CascadeShadows::getLightDirection() const {
	return getComponent<TransformComponent>()->getForward();
}


const glm::vec2& CascadeShadows::getResolution() const {
	return resolution;
}

float CascadeShadows::getCameraFarPlane() {
	return 500.f;
}

unsigned CascadeShadows::getShadowMapTextureArray() {
	return lightDepthMaps;
}

void CascadeShadows::cacheMatrices() {
	lightMatricesCache = getLightSpaceMatrices();
}

void CascadeShadows::clearCacheMatrices() {
	lightMatricesCache.clear();
}



void CascadeShadows::drawCascadeVolumeVisualizers(const std::vector<glm::mat4>& lightMatrices, GameEngine::ShaderModule::ShaderBase* shader) {
	visualizerVAOs.resize(8);
	visualizerEBOs.resize(8);
	visualizerVBOs.resize(8);

	const GLuint indices[] = {
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

	const glm::vec4 colors[] = {
		{1.0, 0.0, 0.0, 0.5f},
		{0.0, 1.0, 0.0, 0.5f},
		{0.0, 0.0, 1.0, 0.5f},
	};

	for (int i = 0; i < lightMatrices.size(); ++i) {
		const auto corners = CascadeShadow::getFrustumCornersWorldSpace(lightMatrices[i]);
		std::vector<glm::vec3> vec3s;
		for (const auto& v : corners) {
			vec3s.push_back(glm::vec3(v));
		}

		glGenVertexArrays(1, &visualizerVAOs[i]);
		glGenBuffers(1, &visualizerVBOs[i]);
		glGenBuffers(1, &visualizerEBOs[i]);

		glBindVertexArray(visualizerVAOs[i]);

		glBindBuffer(GL_ARRAY_BUFFER, visualizerVBOs[i]);
		glBufferData(GL_ARRAY_BUFFER, vec3s.size() * sizeof(glm::vec3), &vec3s[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, visualizerEBOs[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

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
