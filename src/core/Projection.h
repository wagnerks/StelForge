#pragma once

#include "mathModule/Forward.h"

namespace SFE::ProjectionModule {
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

		const SFE::Math::Mat4& getProjectionsMatrix() const;
		virtual void initProjection() = 0;

		float getNear() const;
		float getFar() const;

		void setNear(float near);
		void setFar(float far);

		void setNearFar(float near, float far);
	protected:
		SFE::Math::Mat4 mProjectionMatrix = {};
	private:

		float mNear = 0.1f;
		float mFar = 1000.f;
	};

	struct OrthoProjection : Projection {
	public:
		OrthoProjection() = default;
		OrthoProjection(Math::Vec2 leftBtm, Math::Vec2 rightTop, float zNear, float zFar);
		void initProjection() override;

		void setLeftBtm(Math::Vec2 point);
		void setRightTop(Math::Vec2 point);

	private:
		Math::Vec2 mLeftBtm = {};
		Math::Vec2 mRightTop = {};
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


