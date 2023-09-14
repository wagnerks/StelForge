#pragma once
#include <fwd.hpp>
#include <vec2.hpp>

#include <vector>
#include "ecsModule/EntityBase.h"
#include "assetsModule/shaderModule/ShaderBase.h"

class CascadeShadows : public ecsModule::Entity<CascadeShadows> {
public:
	CascadeShadows(size_t entID, glm::vec2 resolution);

	const std::vector<glm::mat4>& getLightSpaceMatrices();


	void cacheMatrices();

	static const std::vector<glm::mat4>& getCacheLightSpaceMatrices();
	static void debugDraw(const std::vector<glm::mat4>& lightSpaceMatrices, const glm::mat4& cameraProjection, const glm::mat4& cameraView);
	static void clearCacheMatrices();
private:
	static void drawCascadeVolumeVisualizers(const std::vector<glm::mat4>& lightMatrices, Engine::ShaderModule::ShaderBase* shader);
	static inline std::vector<glm::mat4> mLightMatricesCache;
};
