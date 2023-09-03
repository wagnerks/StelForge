#pragma once
#include "Light.h"
#include "core/Projection.h"

namespace Engine::LightsModule {
	class DirectionalOrthoLight : public Light {
	public:
		DirectionalOrthoLight(size_t entID, int aShadowWidth = 2048, int aShadowHeight = 2048, float zNear = 1.f, float zFar = 30.f);
		~DirectionalOrthoLight();
		void preDraw();
		void postDraw();
		unsigned getDepthMapTexture() const;
		const glm::mat4& getProjectionViewMatrix() const;
		const ProjectionModule::OrthoProjection& getProjection();
	private:
		ProjectionModule::OrthoProjection lightProjection;
		int shadowWidthResolution;
		int shadowHeightResolution;

		float shadowWidth = 0.f;
		float shadowHeight = 0.f;

		glm::mat4 lightPV = {};

		unsigned int depthMap = 0;
		unsigned int depthMapFBO = 0;
	};
}

