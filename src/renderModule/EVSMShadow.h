#pragma once
#include <glm.hpp>

#include "ecsModule/EntityBase.h"


//exponential variance shadow map
class EVSMShadow : public ecsModule::Entity<EVSMShadow> {
public:
	EVSMShadow(size_t entID);
	~EVSMShadow();

	void preDraw();
	void postDraw();
//private:
	int shadowWidth;
	int shadowHeight;
	float nearPlane = 1.f;
	float farPlane = 50.f;

	glm::mat4 lightSpaceMatrix = {};

	unsigned int depthMap = 0;
	unsigned int depthMapFBO = 0;


	glm::vec2 exponents = {1.f, 2.f};
	float u_LightBleedReduction = 0.6f;
	float u_VarianceMinLimit = 0.f;
	int u_EVSMMode = 0;
	float u_ShadowBias = 0.01f;

	int gaussAlgorithm = 2;
	glm::vec2 blurAmount = {1.5f,1.5f};
};
