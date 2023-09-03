#pragma once
#include <fwd.hpp>

namespace Engine::RenderModule {
	class Utils {
	public:
		//angles in radians
		static void CalculateEulerAnglesFromView(const glm::mat4& view, float& yaw, float& pitch, float& roll);
		static void renderQuad();
		static void renderQuad(float x1, float y1, float x2, float y2);
		static void renderCube();
		static void initCubeVAO();
		static void renderXYZ(float length);
		inline static unsigned cubeVAO = 0;
		inline static unsigned cubeVBO = 0;
	};
}
