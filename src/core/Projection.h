#pragma once
#include <vec3.hpp>
#include <mat4x4.hpp>

namespace GameEngine::ProjectionModule {
	struct Projection {
		virtual ~Projection() = default;
		const glm::mat4& getProjectionsMatrix() const;
		virtual void initProjection() = 0;
		float getZNear();
		float getZFar();
		void setZNear(float near);
		void setZFar(float far);
	protected:
		Projection() = default;
		Projection(float zNear, float zFar) : zNear(zNear), zFar(zFar){};
		glm::mat4 projection = {};
		float zNear = 0.1f;
		float zFar = 1000.f;
	};

	struct OrthoProjection : public Projection {
		OrthoProjection() = default;
		OrthoProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar);
		void initProjection() override;
		void setLeftBtm(glm::vec2 point);
		void setRightTop(glm::vec2 point);
		void setProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar);
	private:
		glm::vec2 leftBtm = {};
		glm::vec2 rightTop = {};
	};

	struct PerspectiveProjection : public Projection {
		PerspectiveProjection() = default;
		PerspectiveProjection(float FOV, float aspect, float zNear, float zFar);
		void setFOV(float FOV);
		float getFOV() const;
		void setAspect(float aspect);
		void initProjection() override;
		void setProjection(float FOV, float aspect, float zNear, float zFar);
		float getAspect() const;;
	private:
		float FOV = 45.0f;
		float aspect = 0.f;
	};
}


