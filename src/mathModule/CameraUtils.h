#pragma once
#include "Forward.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Types.h"
#include "renderModule/Renderer.h"
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
	inline Ray calcCameraRay(ecss::EntityId cameraId, const Vec2& screenCoords = { RenderModule::Renderer::SCR_WIDTH * 0.5f, RenderModule::Renderer::SCR_HEIGHT * 0.5f}) {
		auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(cameraId);
		if (!cameraComp) {
			return {};
		}

		auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(cameraId);

		auto mProjection = cameraComp->getProjection().getProjectionsMatrix();
		auto mView = cameraTransform->getViewMatrix();


		
		auto cameraPos = cameraTransform->getPos(true);
		float normalizedX = (2.0f * screenCoords.x) / RenderModule::Renderer::SCR_WIDTH - 1.0f;
		float normalizedY = 1.0f - (2.0f * screenCoords.y) / RenderModule::Renderer::SCR_HEIGHT;
		auto clipCoords = Math::Vec4(normalizedX, normalizedY, -1.0, 1.0);
		auto ndc = Math::inverse(mProjection) * clipCoords;
		ndc /= ndc.w;
		auto viewCoords = Math::Vec3(Math::inverse(mView) * Math::Vec4(ndc.x, ndc.y, ndc.z, 1.0));
		auto rayDirection = Math::normalize(viewCoords - cameraPos);

		return { viewCoords, rayDirection };
	}

	inline Vec3 globalToScreen(Vec4 globalCoords, const Mat4& ProjectView) {
		globalCoords = ProjectView * globalCoords;
		globalCoords.xyz /= globalCoords.w;
		globalCoords.xyz = (globalCoords.xyz + 1.f) / 2.f;
		globalCoords.x *= RenderModule::Renderer::SCR_WIDTH;
		globalCoords.y *= RenderModule::Renderer::SCR_HEIGHT;
		globalCoords.y = RenderModule::Renderer::SCR_HEIGHT - globalCoords.y;

		return globalCoords;
	}

	inline Vec3 globalToScreen(const Vec4& globalCoords) {
		auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera());
		if (!cameraComp) {
			return {};
		}

		auto cameraTransform = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera());

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
			if (cross(a, b) < Vec3(0.0f)) {
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
		auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(cameraId);
		if (!cameraComp) {
			return {};
		}

		auto mProjection = ECSHandler::registry().getComponent<CameraComponent>(cameraId)->getProjection().getProjectionsMatrix();
		auto mView = ECSHandler::registry().getComponent<TransformComponent>(cameraId)->getViewMatrix();

		return FrustumModule::createFrustum(mProjection * mView);
	}
}
