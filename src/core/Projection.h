#pragma once
#include <vec3.hpp>
#include <mat4x4.hpp>

namespace GameEngine::ProjectionModule {
	enum eProjectionType {
		NONE,
		ORTHO,
		PERSPECTIVE
	};

	struct Projection {
		Projection() = default;
		Projection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar);
		Projection(float FOV, float aspect, float zNear, float zFar);

		virtual ~Projection() = default;

		const glm::mat4& getProjectionsMatrix() const;
		void initProjection();
		float getNear() const;
		float getFar() const;
		void setNear(float near);
		void setFar(float far);


		void setFOV(float FOV);
		float getFOV() const;
		void setAspect(float aspect);
		float getAspect() const;
		void setProjection(float FOV, float aspect, float zNear, float zFar);
		

		void setLeftBtm(glm::vec2 point);
		void setRightTop(glm::vec2 point);
		void setProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar);
	private:
		glm::mat4 mProjectionMatrix = {};
		float mNear = 0.1f;
		float mFar = 1000.f;

		//all in one to avoid memory slicing and casting
		float mFOV = 45.0f;
		float mAspect = 0.f;

		glm::vec2 mLeftBtm = {};
		glm::vec2 mRightTop = {};

		eProjectionType mType = NONE;
	};
}


