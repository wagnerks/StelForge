#pragma once

#include "mathModule/Forward.h"

namespace SFE::MathModule {
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

		const SFE::Math::Mat4& getProjectionsMatrix() const;

		float getNear() const;
		float getFar() const;

		void setNear(float near);
		void setFar(float far);

		void setNearFar(float near, float far);
	protected:
		Projection(eProjectionType type) : mType(type) {}
		Projection(eProjectionType type, float near, float far) : mType(type), mNear(near), mFar(far) {}

		/**
		 * @brief Rebuild the matrix after a change made through the base.
		 *
		 * Dispatches on mType rather than through a vtable. A virtual function here would make
		 * every component that stores a projection non-trivially-copyable -- and with it, that
		 * component's whole ecss array -- while nothing in the engine has ever owned a
		 * Projection through a base pointer.
		 */
		void recomputeProjection();

		SFE::Math::Mat4 mProjectionMatrix = {};
		eProjectionType mType = NONE;
	private:

		float mNear = 0.1f;
		float mFar = 1000.f;
	};

	struct OrthoProjection : Projection {
	public:
		OrthoProjection() : Projection(ORTHO) {}
		OrthoProjection(Math::Vec2 leftBtm, Math::Vec2 rightTop, float zNear, float zFar);
		void initProjection();

		void setLeftBtm(Math::Vec2 point);
		void setRightTop(Math::Vec2 point);

	private:
		Math::Vec2 mLeftBtm = {};
		Math::Vec2 mRightTop = {};
	};

	struct PerspectiveProjection : Projection {
	public:
		PerspectiveProjection() : Projection(PERSPECTIVE) {}
		PerspectiveProjection(float FOV, float aspect, float zNear, float zFar);
		void initProjection();

		void setFOV(float FOV);
		float getFOV() const;
		void setAspect(float aspect);
		float getAspect() const;

	private:
		float mFOV = 45.0f;
		float mAspect = 0.f;
	};
}


