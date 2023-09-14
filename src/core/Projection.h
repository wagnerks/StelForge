#pragma once
#include <vec3.hpp>
#include <mat4x4.hpp>

namespace Engine::ProjectionModule {
	enum eProjectionType {
		NONE,
		ORTHO,
		PERSPECTIVE
	};

	struct Projection {
		friend bool operator==(const Projection& lhs, const Projection& rhs) {
			return lhs.mProjectionMatrix == rhs.mProjectionMatrix;
		}

		friend bool operator!=(const Projection& lhs, const Projection& rhs) {
			return !(lhs == rhs);
		}

		Projection() = default;
		Projection(float near, float far) : mNear(near), mFar(far) {}
		virtual ~Projection() = default;

		const glm::mat4& getProjectionsMatrix() const;
		virtual void initProjection() = 0;

		float getNear() const;
		float getFar() const;

		void setNear(float near);
		void setFar(float far);
	protected:
		glm::mat4 mProjectionMatrix = {};
	private:

		float mNear = 0.1f;
		float mFar = 1000.f;
	};

	struct OrthoProjection : Projection {
	public:
		OrthoProjection() = default;
		OrthoProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar);
		void initProjection() override;

		void setLeftBtm(glm::vec2 point);
		void setRightTop(glm::vec2 point);

	private:
		glm::vec2 mLeftBtm = {};
		glm::vec2 mRightTop = {};
	};

	struct PerspectiveProjection : Projection {
	public:
		PerspectiveProjection() = default;
		PerspectiveProjection(float FOV, float aspect, float zNear, float zFar);
		void initProjection() override;

		void setFOV(float FOV);
		float getFOV() const;
		void setAspect(float aspect);
		float getAspect() const;

	private:
		float mFOV = 45.0f;
		float mAspect = 0.f;
	};
}


