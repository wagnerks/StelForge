#pragma once
#include <fwd.hpp>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vec4.hpp>
#include <vector>
#include "ecsModule/EntityBase.h"
#include "core/BoundingVolume.h"
#include "shaderModule/ShaderBase.h"
#include "core/Projection.h"

class CascadeShadow : public ecsModule::Entity<CascadeShadow> {
public:
	CascadeShadow(size_t entID);

	static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
	static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projView);
	glm::mat4 getLightSpaceMatrix(const std::vector<glm::vec4>& corners);

	// Tune this parameter according to the scene
	float mZMult = 2000.0f;

	float mBiasMultiplier = 0.0f;
	float mTexelsMultiplier = 0.05f;
	glm::mat4 proj = {};
private:

};

class CascadeShadows : public ecsModule::Entity<CascadeShadows> {
public:
	CascadeShadows(size_t entID, glm::vec2 resolution);
	~CascadeShadows();
	void init();
	void preDraw();
	void postDraw();
	void debugDraw();

	std::vector<glm::mat4> getLightSpaceMatrices();
	const std::vector<float>& getShadowCascadeLevels();
	const glm::vec3& getLightPosition() const;
	glm::vec3 getLightDirection() const;
	const glm::vec2& getResolution() const;
	float getCameraFarPlane();
	unsigned getShadowMapTextureArray();
	void cacheMatrices();
	void clearCacheMatrices();
	
	
	std::vector<CascadeShadow*> shadows;

	float sunProgress = 0.f;
private:
	unsigned lightFBO;
	unsigned lightDepthMaps;
	unsigned matricesUBO;
	
	glm::vec2 resolution;

	std::vector<unsigned> visualizerVAOs;
	std::vector<unsigned> visualizerVBOs;
	std::vector<unsigned> visualizerEBOs;

	void drawCascadeVolumeVisualizers(const std::vector<glm::mat4>& lightMatrices, GameEngine::ShaderModule::ShaderBase* shader);

	std::vector<glm::mat4> lightMatricesCache;
	std::vector<float> shadowCascadeLevels;

	
};
