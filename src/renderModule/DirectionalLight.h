#pragma once
#include "Light.h"

namespace GameEngine::LightsModule {
	class DirectionalLight : public Light {
	public:
		DirectionalLight(size_t aShadowWidth = 4096, size_t aShadowHeight = 4096, float aNearPlane = 1.f, float aFarPlane = 30.f);
		~DirectionalLight() override;
		void preDraw();
		void postDraw();
		unsigned getDepthMapTexture();
		const glm::mat4& getLightSpaceProjection();
		float getNearPlane();
		float getFarPlane();
	private:

		size_t shadowWidth;
		size_t shadowHeight;
		float nearPlane;
		float farPlane;
		glm::mat4 lightSpaceMatrix = {};

		unsigned int depthMap = 0;
		unsigned int depthMapFBO = 0;
	};
}

