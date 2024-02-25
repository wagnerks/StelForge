#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Batcher.h"

namespace SFE::Render {
	enum RenderMode : uint32_t {
		POINTS = GL_POINTS,
		LINES = GL_LINES,
		LINE_LOOP = GL_LINE_LOOP,
		LINE_STRIP = GL_LINE_STRIP,
		TRIANGLES = GL_TRIANGLES,
		TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
		TRIANGLE_FAN = GL_TRIANGLE_FAN,
		POLYGON = GL_POLYGON,
		QUADS = GL_QUADS
	};

	enum class RenderDataType {
		UNSIGNED_BYTE = GL_UNSIGNED_BYTE,
		UNSIGNED_SHORT = GL_UNSIGNED_SHORT,
		UNSIGNED_INT = GL_UNSIGNED_INT,
	};

	class Renderer : public Singleton<Renderer> {
		friend Renderer;
	public:

		~Renderer();
		void draw();
		void postDraw();
		void init();

		Batcher* getBatcher() { return mBatcher; };

		static void drawArrays(RenderMode mode, GLsizei size, GLint first = 0);
		static void drawElements(RenderMode mode, GLsizei size, RenderDataType type, const void* place = nullptr);
		static void drawElementsInstanced(RenderMode mode, GLsizei size, RenderDataType type, GLsizei instancesCount, const void* place = nullptr);
		static void drawArraysInstancing(RenderMode mode, GLsizei size, GLsizei instancesCount, GLint first = 0);

		static void drawVertices(RenderMode mode, const unsigned vaoId, size_t verticesCount, size_t indicesCount = 0, size_t instancesCount = 1, RenderDataType indicesType = RenderDataType::UNSIGNED_INT, const void* place = nullptr) {
			if (vaoId == 0 || verticesCount == 0) {
				assert(false);
				return;
			}

			VertexArray::bindArray(vaoId);
			if (instancesCount) {
				if (indicesCount) {
					drawElementsInstanced(mode, static_cast<int>(indicesCount), indicesType, static_cast<int>(instancesCount), place);
				}
				else {
					drawArraysInstancing(mode, static_cast<int>(verticesCount), static_cast<int>(instancesCount));
				}
			}
			else {
				if (indicesCount) {
					drawElements(mode, static_cast<int>(indicesCount), indicesType, place);
				}
				else {
					drawArrays(mode, static_cast<int>(verticesCount));
				}
			}
			VertexArray::bindDefault();
		}

		inline static int SCR_WIDTH = 1920;
		inline static int SCR_HEIGHT = 1080;
		inline static int SCR_RENDER_W = SCR_WIDTH;
		inline static int SCR_RENDER_H = SCR_HEIGHT;

		inline static float SCR_RENDER_SCALE_W = 1.f;
		inline static float SCR_RENDER_SCALE_H = 1.f;

		inline static float drawDistance = 5000.f;
		inline static float nearDistance = 1.f;
		inline static size_t mDrawCallsCount = 0;
		inline static size_t mDrawVerticesCount = 0;
	private:
		Batcher* mBatcher = nullptr;
	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool mGLFWInited = false;
	};
}

