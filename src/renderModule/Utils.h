#pragma once

#include <map>
#include <unordered_map>

#include "glad/glad.h"
#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"


namespace SFE::RenderModule {
	class Utils {
	public:
		struct LineData {
			Math::Vec4 color;
			float thickness = 1.f;
			uint32_t renderType = GL_LINES;
		};
		inline static std::vector<std::pair<LineData, std::vector<Math::Vec3>>> renderVertices;

		struct TriangleData {
			Math::Vec4 color;
		};

		struct Triangle {
			Math::Vec3 A;
			Math::Vec3 B;
			Math::Vec3 C;
		};

		inline static std::vector<std::pair<TriangleData, std::vector<Triangle>>> renderTriangles;

		static std::vector<Math::Vec3>& getVerticesArray(const Math::Vec4& color, float thickness, uint32_t renderType);
		static std::vector<Triangle>& getTrianglesArray(const TriangleData& data);

		//angles in radians
		static void CalculateEulerAnglesFromView(const Math::Mat4& view, float& yaw, float& pitch, float& roll);
		static void renderQuad();
		static void renderQuad2();
		static void renderQuad(float x1, float y1, float x2, float y2);
		static void renderCube();
		static void initCubeVAO();
		static void renderXYZ(float length);

		static void renderLine(const Math::Vec3& begin, const Math::Vec3& end, const Math::Vec4& color, float thickness = 1.f);
		static void renderPolygon();

		
		static void renderTriangle(const Triangle& triangle, const Math::Vec4& color);
		static void renderTriangle(const Triangle& triangle, const Math::Mat4& transform, const Math::Vec4& color);

		static void renderCube(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);
		static void renderQuad(const Math::Vec3& min, const Math::Vec3& max, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);
		static void renderCubeMesh(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);
		static void renderSphere(const Math::Vec3& center, float radius);
		static void renderCircle(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, const Math::Vec4& color, int numSegments = 16, uint32_t renderType = GL_LINE_LOOP);
		static void renderCircleFilled(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, const Math::Vec4& color, int numSegments, float startAngle = 0.f, float endAngle = 360.f);

		static void renderCone(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, float height, const Math::Vec4& color, int numSegments = 64);

		static void renderCapsule(const Math::Vec3& start, const Math::Vec3& end, float radius);
		static void renderCamera();
		static void renderPointLight(float near, float far, const Math::Vec3& pos);

		inline static unsigned cubeVAO = 0;
		inline static unsigned cubeVBO = 0;
	};
}
