#pragma once

#include <cstdint>
#include <glad/glad.h>

#include "StateStack.h"
#include "VertexArray.h"

namespace SFE::GLW {
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

	enum class ColorBit {
		DEPTH = GL_DEPTH_BUFFER_BIT,
		STENCIL = GL_STENCIL_BUFFER_BIT,
		COLOR = GL_COLOR_BUFFER_BIT,
		DEPTH_STENCIL = DEPTH | STENCIL,
		DEPTH_COLOR = DEPTH | COLOR,
		STENCIL_COLOR = STENCIL | COLOR,
		ALL = DEPTH | STENCIL | COLOR,
	};

	constexpr void clear(ColorBit mask) {
		glClear(static_cast<int>(mask));
	}
	constexpr inline void setClearColor(float val, float a = 0.f) {
		glClearColor(val, val, val, a);
	}

	constexpr inline void setClearColor(float r, float g, float b, float a = 0.f) {
		glClearColor(r, g, b, a);
	}

	constexpr inline void drawArrays(RenderMode mode, GLsizei size, GLint first = 0) {
		glDrawArrays(mode, first, size);
	}

	constexpr inline void drawElements(RenderMode mode, GLsizei size, RenderDataType type = RenderDataType::UNSIGNED_BYTE, const void* place = nullptr) {
		glDrawElements(mode, size, static_cast<GLenum>(type), place);
	}

	constexpr inline void drawElementsInstanced(RenderMode mode, GLsizei size, RenderDataType type = RenderDataType::UNSIGNED_BYTE, GLsizei instancesCount = 0, const void* place = nullptr) {
		glDrawElementsInstanced(mode, size, static_cast<GLenum>(type), place, instancesCount);
	}

	constexpr inline void drawArraysInstancing(RenderMode mode, GLsizei size, GLsizei instancesCount, GLint first = 0) {
		glDrawArraysInstanced(mode, first, size, instancesCount);
	}

	constexpr inline void drawVerticesW(RenderMode mode, size_t verticesCount, size_t indicesCount = 0, size_t instancesCount = 0, RenderDataType indicesType = RenderDataType::UNSIGNED_INT, const void* place = nullptr) {
		if (verticesCount == 0) {
			return;
		}

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
	}

	constexpr inline void drawVertices(RenderMode mode, const unsigned vaoId, size_t verticesCount, size_t indicesCount = 0, size_t instancesCount = 1, RenderDataType indicesType = RenderDataType::UNSIGNED_INT, const void* place = nullptr) {
		if (vaoId == 0 || verticesCount == 0) {
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

	struct LineWidth : StateStack<float, LineWidth> {
		void apply(float* data) override {
			if (!data) {
				glLineWidth(1.f);
			}
			else {
				glLineWidth(*data);
			}
		}
	};

	enum class PolygonFace {
		FRONT_AND_BACK = GL_FRONT_AND_BACK,
		FRONT = GL_FRONT,
		BACK = GL_BACK
	};

	enum class PolygonType {
		POINT = GL_POINT,
		LINE = GL_LINE,
		FILL = GL_FILL
	};

	template<PolygonFace FACE>
	struct PolygonMode : StateStack<PolygonType, PolygonMode<FACE>> {
		void apply(PolygonType* data) override {
			if (!data) {
				glPolygonMode(static_cast<int>(FACE), static_cast<int>(PolygonType::FILL));
			}
			else {
				glPolygonMode(static_cast<int>(FACE), static_cast<int>(*data));
			}
		}
	};

}
