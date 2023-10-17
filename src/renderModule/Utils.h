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

		static void renderLine(glm::vec3& begin, glm::vec3& end);
		static void renderCube(const glm::vec3& LT, const glm::vec3& RB);

		static void renderCamera();
		static void renderPointLight(float near, float far);

		inline static unsigned cubeVAO = 0;
		inline static unsigned cubeVBO = 0;
	};
}
