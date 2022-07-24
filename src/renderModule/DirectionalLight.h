#pragma once
#include "Light.h"

namespace GameEngine::LightsModule {
	class DirectionalLight : public Light {
	public:
		DirectionalLight(size_t aShadowWidth = 1024, size_t aShadowHeight = 1024, float aNearPlane = 1.f, float aFarPlane = 500.f);
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

