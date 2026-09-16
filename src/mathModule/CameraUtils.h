#pragma once
#include "Forward.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Types.h"
#include "..\windowsModule\Window.h"
#include "systemsModule/systems/CameraSystem.h"

namespace SFE::Math {
	struct Line {
		Vec3 a;
		Vec3 b;
	};

	struct Ray {
		Vec3 a;
		Vec3 direction;
	};

	// screenCoords - is the point on "camera lense" from which ray casted
	inline Ray calcMouseRay(ecss::EntityId cameraId, const Vec2& screenCoords) {
		auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(cameraId);
		if (!cameraComp) {
			return {};
		}

		auto cameraTransform = ECSHandler::registry().pinComponent<const TransformComponent>(cameraId);

		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		const float normalizedX = (2.0f * screenCoords.x) / screen.width - 1.0f;
		const float normalizedY = 1.0f - (2.0f * screenCoords.y) / screen.height;

		// one inverse of the combined matrix instead of inverting projection and view separately
		const auto invProjView = Math::inverse(cameraComp->getProjection().getProjectionsMatrix() * cameraTransform->getViewMatrix());
		auto nearPoint = invProjView * Math::Vec4(normalizedX, normalizedY, -1.f, 1.f);
		const Vec3 worldPoint = nearPoint.xyz / nearPoint.w;

		return { worldPoint, Math::normalize(worldPoint - cameraTransform->getPos(true)) };
	}

	inline Ray calcMouseRay(ecss::EntityId cameraId) {
		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		return calcMouseRay(cameraId, { screen.width * 0.5f, screen.height * 0.5f });
	}

	inline Vec3 globalToScreen(Vec4 globalCoords, const Mat4& ProjectView) {
		globalCoords = ProjectView * globalCoords;
		globalCoords.xyz /= globalCoords.w;
		globalCoords.xyz = (globalCoords.xyz + 1.f) / 2.f;
		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		globalCoords.x *= screen.width;
		globalCoords.y = screen.height * (1.f - globalCoords.y);

		return globalCoords;
	}

	inline Vec3 globalToScreen(const Vec4& globalCoords) {
		auto cameraSys = ECSHandler::getSystem<SystemsModule::CameraSystem>();
		if (!cameraSys) {
			return {};
		}
		
		auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(cameraSys->getCurrentCamera());
		if (!cameraComp) {
			return {};
		}

		auto cameraTransform = ECSHandler::registry().pinComponent<const TransformComponent>(cameraSys->getCurrentCamera());

		const auto& mProjection = cameraComp->getProjection().getProjectionsMatrix();
		const auto& mView = cameraTransform->getViewMatrix();
		return globalToScreen(globalCoords, mProjection * mView);

	}

	inline float calcAngleOnCircle(float hordeLen, float radius) {
		return 2.f * std::asin(hordeLen / (2.f * radius));
	}

	inline float calcAngleBetweenVectors(const Vec3& a, const Vec3& b) {
		const auto dot = Math::dot(a, b);
		const auto magA = length(a);
		const auto magB = length(b);

		// Ensure denominators are not zero to avoid division by zero issues
		if (magA > 0.0f && magB > 0.0f) {
			auto cosTheta = dot / (magA * magB);

			// Clamp cosTheta to the valid range [-1, 1]
			cosTheta = std::max(-1.0f, std::min(1.0f, cosTheta));

			auto angle = std::acos(cosTheta);
			if (cross(a, b).z < 0.0f) {
				// Adjust the angle for the 180 to 360 degree range
				angle = 2.0f * Math::pi<float>() - angle;
			}
			// Calculate the angle in radians
			return angle;
		}

		return 0.0f;  // Return 0 if magnitudes are zero
	}

	//    a*
	//    /
	//   /
	//b *------* c
	inline float calcAngleBetweenVectors(const Vec3& a, const Vec3& b, const Vec3& c) {
		return calcAngleBetweenVectors(b - a, b - c);
	}

	inline FrustumModule::Frustum createFrustum(ecss::EntityId cameraId) {
		auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(cameraId);
		if (!cameraComp) {
			return {};
		}

		auto& mProjection = cameraComp->getProjection().getProjectionsMatrix();
		auto mView = ECSHandler::registry().pinComponent<const TransformComponent>(cameraId)->getViewMatrix();

		return FrustumModule::createFrustum(mProjection * mView);
	}
}
