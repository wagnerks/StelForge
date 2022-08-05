#pragma once
#include <fwd.hpp>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vec4.hpp>
#include <vector>

#include "shaderModule/ShaderBase.h"
#include "core/Projection.h"

class CascadeShadows {
public:
	CascadeShadows(glm::vec2 resolution);
	~CascadeShadows();
	void init();
	void preDraw();
	void postDraw();
	void debugDraw();

	std::vector<glm::mat4> getLightSpaceMatrices();
	const std::vector<float>& getShadowCascadeLevels();
	const glm::vec3& getLightPosition() const;
	glm::vec3 getLightDirection() const;
	const glm::vec3& getLightColor() const;
	void setLightColor(const glm::vec3& color);
	void setLightPosition(const glm::vec3& position);
	const glm::vec2& getResolution() const;
	float getCameraFarPlane();
	unsigned getShadowMapTextureArray();
	void cacheMatrices();
	void clearCacheMatrices();
	float getBias() const;
	void setBias(float cascade_bias) { bias = cascade_bias;};
	float sunProgress = 0.f;
private:
	unsigned lightFBO;
	unsigned lightDepthMaps;
	unsigned matricesUBO;
	
	glm::vec2 resolution;
	glm::vec3 lightPos;
	glm::vec3 lightDir;

	glm::vec3 lightColor;

	std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
	glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane);
	std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projview);
	
	std::vector<unsigned> visualizerVAOs;
	std::vector<unsigned> visualizerVBOs;
	std::vector<unsigned> visualizerEBOs;

	void drawCascadeVolumeVisualizers(const std::vector<glm::mat4>& lightMatrices, GameEngine::ShaderModule::ShaderBase* shader);


	std::vector<glm::mat4> lightMatricesCache;
	std::vector<float> shadowCascadeLevels;

	GameEngine::ProjectionModule::PerspectiveProjection projection;
	float bias = 0.f;

};
