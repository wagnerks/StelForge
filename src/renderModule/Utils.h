#pragma once

#include <map>
#include <unordered_map>

#include "mathModule/Forward.h"


namespace SFE::RenderModule {
	class Utils {
	public:
		inline static std::vector<std::pair<Math::Vec4, std::vector<Math::Vec3>>> renderVertices;
		static std::vector<Math::Vec3>& getVerticesArray(const Math::Vec4& color);

		//angles in radians
		static void CalculateEulerAnglesFromView(const Math::Mat4& view, float& yaw, float& pitch, float& roll);
		static void renderQuad();
		static void renderQuad2();
		static void renderQuad(float x1, float y1, float x2, float y2);
		static void renderCube();
		static void initCubeVAO();
		static void renderXYZ(float length);

		static void renderLine(const Math::Vec3& begin, const Math::Vec3& end, const Math::Vec4& color);

		static void renderCube(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);
		static void renderSphere(const Math::Vec3& center, float radius);

		static void renderCapsule(const Math::Vec3& start, const Math::Vec3& end, float radius);
		static void renderCamera();
		static void renderPointLight(float near, float far, const Math::Vec3& pos);

		inline static unsigned cubeVAO = 0;
		inline static unsigned cubeVBO = 0;
	};
}
