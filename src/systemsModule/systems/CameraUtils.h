#pragma once

#include "CameraSystem.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "ecss/Types.h"
#include "mathModule/Geometry.h"
#include "windowsModule/Window.h"

// Math that needs a camera entity and the window; the pure parts live in mathModule/Geometry.h.
namespace SFE::CameraUtils {
	// screenCoords - the pixel the ray goes through, top-left origin
	inline Math::Ray calcMouseRay(ecss::EntityId cameraId, const Math::Vec2& screenCoords) {
		auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(cameraId);
		auto cameraTransform = ECSHandler::registry().pinComponent<const TransformComponent>(cameraId);
		if (!cameraComp || !cameraTransform) {
			return {};
		}

		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		const auto projView = cameraComp->getProjection().getProjectionsMatrix() * cameraTransform->getViewMatrix();
		return Math::screenToWorldRay(projView, screenCoords, { screen.width, screen.height }, cameraTransform->getPos(true));
	}

	// through the centre of the screen
	inline Math::Ray calcMouseRay(ecss::EntityId cameraId) {
		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		return calcMouseRay(cameraId, { screen.width * 0.5f, screen.height * 0.5f });
	}

	inline FrustumModule::Frustum createFrustum(ecss::EntityId cameraId) {
		auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(cameraId);
		auto cameraTransform = ECSHandler::registry().pinComponent<const TransformComponent>(cameraId);
		if (!cameraComp || !cameraTransform) {
			return {};
		}

		return FrustumModule::createFrustum(cameraComp->getProjection().getProjectionsMatrix() * cameraTransform->getViewMatrix());
	}
}
