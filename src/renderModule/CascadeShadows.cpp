#include "CascadeShadows.h"

#include <mat4x4.hpp>
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>

#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "glad/glad.h"
#include "logsModule/logger.h"
#include "shaderModule/ShaderController.h"

CascadeShadows::CascadeShadows(glm::vec2 resolution) : resolution(resolution) {
	init();
}

CascadeShadows::~CascadeShadows() {
	glDeleteFramebuffers(1, &lightFBO);
	glDeleteTextures(1, &lightDepthMaps);
	glDeleteBuffers(1, &matricesUBO);
}

void CascadeShadows::init() {
	projection = GameEngine::ProjectionModule::PerspectiveProjection(glm::radians(GameEngine::Engine::getInstance()->getCamera()->cameraView.getFOV()), static_cast<float>(GameEngine::RenderModule::Renderer::SCR_WIDTH) / static_cast<float>(GameEngine::RenderModule::Renderer::SCR_HEIGHT), 0.01f, 500.f);

	shadowCascadeLevels = { 500.f / 45.0f, 500.f / 25.0f, 500.f / 10.0f, 500.f / 2.0f };

	lightPos = glm::vec3(20.0f, 50, 20.0f);
	lightDir = glm::normalize(glm::vec3(20.0f, 50, 20.0f));

	lightColor = glm::vec3(1.f);


	glGenFramebuffers(1, &lightFBO);
    
	glGenTextures(1, &lightDepthMaps);
	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
	glTexImage3D(
	    GL_TEXTURE_2D_ARRAY,
	    0,
	    GL_DEPTH_COMPONENT16,
	    static_cast<int>(resolution.x),
	    static_cast<int>(resolution.y),
	    int(shadowCascadeLevels.size()) + 1,
	    0,
	    GL_DEPTH_COMPONENT,
	    GL_FLOAT,
	    nullptr);
    
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	    
	constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);
	    
	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	

	int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		GameEngine::LogsModule::Logger::LOG_ERROR("FRAMEBUFFER::CascadeShadow Framebuffer is not complete!");
	}
	    
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// configure UBO
    // --------------------

    glGenBuffers(1, &matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void CascadeShadows::preDraw() {
	const auto lightMatrices = getLightSpaceMatrices();
	glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4x4) * lightMatrices.size(), &lightMatrices[0]);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

	auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, lightDepthMaps, 0);
	glViewport(0, 0, static_cast<int>(resolution.x), static_cast<int>(resolution.y));
	glClear(GL_DEPTH_BUFFER_BIT);
	glCullFace(GL_FRONT);  // peter panning
}
void CascadeShadows::postDraw() {
	glCullFace(GL_BACK);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadeShadows::debugDraw() {
	if (!lightMatricesCache.empty()){
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		auto debugCascadeShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugCascadeShader.vs", "shaders/debugCascadeShader.fs");
		debugCascadeShader->use();
		debugCascadeShader->setMat4("projection", GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix());
		debugCascadeShader->setMat4("view", GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getViewMatrix());
		drawCascadeVolumeVisualizers(lightMatricesCache, debugCascadeShader);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);
	}
}


std::vector<glm::vec4> CascadeShadows::getFrustumCornersWorldSpace(const glm::mat4& projview) {
	const auto inv = glm::inverse(projview);

	std::vector<glm::vec4> frustumCorners;
	for (unsigned int x = 0; x < 2; ++x) {
		for (unsigned int y = 0; y < 2; ++y) {
			for (unsigned int z = 0; z < 2; ++z) {
				const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
				frustumCorners.push_back(pt / pt.w);
			}
		}
	}

	return frustumCorners;
}

std::vector<glm::vec4> CascadeShadows::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
    return getFrustumCornersWorldSpace(proj * view);
}



glm::mat4 CascadeShadows::getLightSpaceMatrix(const float nearPlane, const float farPlane) {
	const auto proj = glm::perspective(
		glm::radians(GameEngine::Engine::getInstance()->getCamera()->cameraView.getFOV()),
		(float)GameEngine::RenderModule::Renderer::SCR_WIDTH / (float)GameEngine::RenderModule::Renderer::SCR_HEIGHT,
		nearPlane,
		farPlane);


	const auto corners = getFrustumCornersWorldSpace(
		proj, GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getViewMatrix());

	glm::vec3 center = glm::vec3(0, 0, 0);
	for (const auto& v : corners) {
		center += glm::vec3(v);
	}
	center /= corners.size();
	lightDir = glm::normalize(lightPos);
	const auto lightView = glm::lookAt(center + lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));

	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::min();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::min();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::min();
	for (const auto& v : corners) {
		const auto trf = lightView * v;
		minX = std::min(minX, trf.x);
		maxX = std::max(maxX, trf.x);
		minY = std::min(minY, trf.y);
		maxY = std::max(maxY, trf.y);
		minZ = std::min(minZ, trf.z);
		maxZ = std::max(maxZ, trf.z);
	}

	// Tune this parameter according to the scene
	constexpr float zMult = 10.0f;
	if (minZ < 0) {
		minZ *= zMult;
	}
	else {
		minZ /= zMult;
	}
	if (maxZ < 0) {
		maxZ /= zMult;
	}
	else {
		maxZ *= zMult;
	}

	const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

	return lightProjection * lightView;
}



std::vector<glm::mat4> CascadeShadows::getLightSpaceMatrices() {
	if (!lightMatricesCache.empty()) {
		return lightMatricesCache;
	}

	std::vector<glm::mat4> ret;
	for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i) {
		if (i == 0) {
			ret.push_back(getLightSpaceMatrix(0.01f, shadowCascadeLevels[i]));
		}
		else if (i < shadowCascadeLevels.size()) {
			ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], shadowCascadeLevels[i]));
		}
		else {
			ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], 500.f));
		}
	}
	return ret;
}

const std::vector<float>& CascadeShadows::getShadowCascadeLevels() {
	return shadowCascadeLevels;
}

const glm::vec3& CascadeShadows::getLightPosition() const {
	return lightPos;
}
glm::vec3 CascadeShadows::getLightDirection() const {
	return  glm::normalize(lightPos); //cause we look at 0 always
}
const glm::vec3& CascadeShadows::getLightColor() const {
	return lightColor;
}

void CascadeShadows::setLightColor(const glm::vec3& color) {
	lightColor = color;
}
void CascadeShadows::setLightPosition(const glm::vec3& position) {
	lightPos = position;
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

float CascadeShadows::getBias() const {
	return bias;
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
		const auto corners = getFrustumCornersWorldSpace(lightMatrices[i]);
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
