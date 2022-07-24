#pragma once
#include <fwd.hpp>

namespace GameEngine::RenderModule {
	class Utils {
	public:
		//angles in radians
		static void CalculateEulerAnglesFromView(const glm::mat4& view, float& yaw, float& pitch, float& roll);
		static void renderQuad();
	};
}
