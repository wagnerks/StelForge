#pragma once

#include <map>
#include <unordered_map>

#include "Renderer.h"
#include "glWrapper/Draw.h"

#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"


namespace SFE::Render {
	class Utils {
	public:
		struct LineData {
			Math::Vec4 color;
			float thickness = 1.f;
			GLW::RenderMode renderType = GLW::RenderMode::LINES;
		};
		inline static std::vector<std::pair<LineData, std::vector<Math::Vec3>>> renderVertices;

		struct TriangleData {
			friend bool operator==(const TriangleData& lhs, const TriangleData& rhs) {
				return lhs.depthTest == rhs.depthTest
					&& lhs.blend == rhs.blend
					&& lhs.cull == rhs.cull;
			}

			friend bool operator!=(const TriangleData& lhs, const TriangleData& rhs) {
				return !(lhs == rhs);
			}

			bool depthTest = false;
			bool blend = true;
			bool cull = false;

		};
		struct LightVertex {
			LightVertex() = default;
			LightVertex(const Math::Vec3& pos) : position(pos) {}

			SFE::Math::Vec3 position;
			SFE::Math::Vec3 normal;
			Math::Vec4 color {1.f};
		};

		struct Triangle {
			Triangle() = default;
			Triangle(const Math::Vec3& a, const Math::Vec3& b, const Math::Vec3& c, const Math::Vec4& color) : A(a), B(b), C(c) {
				A.color = color;
				B.color = color;
				C.color = color;
				recalculateNormal();
			}

			void recalculateNormal() {
				const auto AB = B.position - A.position;
				const auto AC = C.position - A.position;

				//Normal of ABC triangle
				const auto Normal = SFE::Math::normalize(SFE::Math::cross(AB, AC));

				A.normal = Normal;
				B.normal = Normal;
				C.normal = Normal;
			}

			LightVertex A;
			LightVertex B;
			LightVertex C;
		};

		inline static std::vector<std::pair<TriangleData, std::vector<Triangle>>> renderTriangles;

		static std::vector<Math::Vec3>& getVerticesArray(const Math::Vec4& color, float thickness, GLW::RenderMode renderType);
		static std::vector<Triangle>& getTrianglesArray(const TriangleData& data);

		//angles in radians
		static void CalculateEulerAnglesFromView(const Math::Mat4& view, float& yaw, float& pitch, float& roll);
		static void renderQuad();
		static void renderQuad2();
		static void renderQuad(float x1, float y1, float x2, float y2);

		static void renderLine(const Math::Vec3& begin, const Math::Vec3& end, const Math::Vec4& color, float thickness = 1.f);
		static void renderBone(const Math::Vec3& begin, const Math::Vec3& end, const Math::Vec4& color, const Math::Quaternion<float>& cameraRotation, const Math::Quaternion<float>& transform);


		static void renderTriangle(const Triangle& triangle);
		static void renderTriangle(const Triangle& triangle, const Math::Mat4& transform, const Math::Vec4& color);

		static void renderCube(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);
		static void renderQuad(const Math::Vec3& min, const Math::Vec3& max, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);

		static void renderCubeMesh(const Math::Vec3& LTN, const Math::Vec3& RBF, const Math::Mat4& rotate, const Math::Vec3& pos, const Math::Vec4& color);
		static void renderSphere(const Math::Vec3& center, float radius);
		static void renderCircle(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, const Math::Vec4& color, int numSegments = 16, float lineThicness = 3.f, GLW::RenderMode renderType = GLW::LINE_LOOP);
		static void renderCircleFilled(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, const Math::Vec4& color, int numSegments, float startAngle = 0.f, float endAngle = 360.f);

		static void renderCone(const Math::Vec3& pos, const Math::Quaternion<float>& quat, const Math::Mat4& scale, float radius, float height, const Math::Vec4& color, int numSegments = 64);

		static void renderCapsule(const Math::Vec3& start, const Math::Vec3& end, float radius);
		static void renderCamera();
		static void renderPointLight(float near, float far, const Math::Vec3& pos);

		inline static unsigned cubeVAO = 0;
		inline static unsigned cubeVBO = 0;
	};
}
