#include "CascadeShadowComponent.h"

#include <ext/matrix_transform.hpp>

#include "CameraComponent.h"
#include "core/BoundingVolume.h"
#include "debugModule/ComponentsDebug.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"

namespace Engine::ComponentsModule {
	std::vector<glm::vec4> CascadeShadowComponent::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
		return getFrustumCornersWorldSpace(proj * view);
	}

	std::vector<glm::vec4> CascadeShadowComponent::getFrustumCornersWorldSpace(const glm::mat4& projView) {
		const auto inv = glm::inverse(projView);

		std::vector<glm::vec4> frustumCorners;
		for (unsigned int x = 0; x < 2; ++x) {
			for (unsigned int y = 0; y < 2; ++y) {
				for (unsigned int z = 0; z < 2; ++z) {
					const glm::vec4 pt = inv * glm::vec4(
						2.0f * static_cast<float>(x) - 1.0f,
						2.0f * static_cast<float>(y) - 1.0f,
						2.0f * static_cast<float>(z) - 1.0f,
						1.0f
					);

					frustumCorners.push_back(pt / pt.w);
				}
			}
		}

		return frustumCorners;
	}

	void CascadeShadowComponent::markDirty() {
		mDirty = true;
	}

	const std::vector<glm::mat4>& CascadeShadowComponent::getLightSpaceMatrices() {
		auto curCamera = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();

		const auto& cameraView = curCamera->getComponent<TransformComponent>()->getViewMatrix();
		const auto& cameraProjection = curCamera->getComponent<CameraComponent>()->getProjection();

		updateCascades(cameraProjection);
		updateLightSpaceMatrices(cameraView);

		return mLightSpaceMatrices;
	}

	void CascadeShadowComponent::serialize(Json::Value& data) {
		data["cascaded_levels"] = Json::arrayValue;

		for (auto level : shadowCascadeLevels) {
			data["cascaded_levels"].append(level);
		}

		data["resolution"] = Json::arrayValue;
		data["resolution"].append(resolution.x);
		data["resolution"].append(resolution.y);

		data["cascades"] = Json::arrayValue;

		for (auto& cascade : cascades) {
			Json::Value cascadeData = Json::objectValue;
			cascadeData["bias"] = cascade.bias;
			cascadeData["samples"] = cascade.samples;

			cascadeData["texel_size"].append(cascade.texelSize.x);
			cascadeData["texel_size"].append(cascade.texelSize.y);

			cascadeData["z_mult"].append(cascade.zMult.x);
			cascadeData["z_mult"].append(cascade.zMult.y);

			data["cascades"].append(cascadeData);
		}
		data["shadow_intensity"] = shadowIntensity;
	}

	void CascadeShadowComponent::deserialize(const Json::Value& data) {
		shadowCascadeLevels.clear();
		for (auto level : data["cascaded_levels"]) {
			shadowCascadeLevels.emplace_back(level.asFloat());
		}

		resolution = { data["resolution"][0].asFloat(), data["resolution"][1].asFloat() };

		auto& cameraProjection = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<CameraComponent>()->getProjection();

		updateCascades(cameraProjection);
		int i = 0;
		for (auto& cascade : cascades) {
			auto& cascadeData = data["cascades"][i++];
			cascade.bias = cascadeData["bias"].asFloat();
			cascade.samples = cascadeData["samples"].asInt();

			cascade.texelSize = { cascadeData["texel_size"][0].asFloat(), cascadeData["texel_size"][1].asFloat() };
			cascade.zMult = { cascadeData["z_mult"][0].asFloat(), cascadeData["z_mult"][1].asFloat() };
		}

		shadowIntensity = data["shadow_intensity"].asFloat();
	}

	void CascadeShadowComponent::updateCascades(const ProjectionModule::PerspectiveProjection& cameraProjection) {
		if (mCameraProjection == cameraProjection && !mDirty) {
			return;
		}
		mCameraProjection = cameraProjection;

		mDirty = false;

		//cascades.clear();
		if (shadowCascadeLevels.empty()) {
			return;
		}

		shadowCascadeLevels.front() = cameraProjection.getNear();
		shadowCascadeLevels.back() = cameraProjection.getFar();

		auto fov = cameraProjection.getFOV();
		auto aspect = cameraProjection.getAspect();

		cascades.resize(shadowCascadeLevels.size() - 1);

		for (size_t i = 1; i < shadowCascadeLevels.size(); ++i) {
			cascades[i - 1].viewProjection = { fov, aspect, shadowCascadeLevels[i - 1], shadowCascadeLevels[i] };
		}

	}

	void CascadeShadowComponent::updateLightSpaceMatrices(const glm::mat4& cameraView) {
		mLightSpaceMatrices.clear();

		for (auto& shadowCascade : cascades) {
			const auto corners = getFrustumCornersWorldSpace(shadowCascade.viewProjection.getProjectionsMatrix(), cameraView);

			auto tc = getEntityComponent<TransformComponent>();

			glm::vec3 directionVector = glm::normalize(tc->getForward());
			glm::vec3 upVector = glm::normalize(tc->getUp());

			glm::vec4 frustumCenter = corners[0];
			for (size_t i = 1u; i < 8; i++) {
				frustumCenter += corners[i];
			}
			frustumCenter /= 8.f;

			//glm::mat4 lightView = glm::lookAt(glm::vec3(frustumCenter) + directionVector, glm::vec3(frustumCenter), upVector);
			auto s = glm::normalize(tc->getRight());
			auto u = glm::normalize(tc->getUp());
			auto f = glm::normalize(tc->getForward());

			auto eye = glm::vec3(frustumCenter) - directionVector;

			glm::mat4 lightView(1);
			lightView[0][0] = s.x;
			lightView[1][0] = s.y;
			lightView[2][0] = s.z;
			lightView[0][1] = u.x;
			lightView[1][1] = u.y;
			lightView[2][1] = u.z;
			lightView[0][2] = -f.x;
			lightView[1][2] = -f.y;
			lightView[2][2] = -f.z;
			lightView[3][0] = -glm::dot(s, eye);
			lightView[3][1] = -glm::dot(u, eye);
			lightView[3][2] = glm::dot(f, eye);

			//lightView = tc->getViewMatrix();

			auto projViewMatrix = getLightSpaceMatrix(corners, lightView, shadowCascade.zMult.x, shadowCascade.zMult.y);

			mLightSpaceMatrices.push_back(projViewMatrix);

			shadowCascade.frustum = Engine::FrustumModule::createFrustum(projViewMatrix);
		}
	}


	glm::mat4 CascadeShadowComponent::getLightSpaceMatrix(const std::vector<glm::vec4>& corners, const glm::mat4& lightView, float nearMultiplier, float farMultiplier) {
		glm::vec4 transform = lightView * corners[0];
		float minX = transform.x;
		float maxX = transform.x;

		float minY = transform.y;
		float maxY = transform.y;

		float minZ = transform.z;
		float maxZ = transform.z;

		for (auto i = 1u; i < corners.size(); i++) {
			transform = lightView * corners[i];

			minX = std::min(minX, transform.x);
			maxX = std::max(maxX, transform.x);

			minY = std::min(minY, transform.y);
			maxY = std::max(maxY, transform.y);

			minZ = std::min(minZ, transform.z);
			maxZ = std::max(maxZ, transform.z);
		}

		auto ortho = Engine::ProjectionModule::OrthoProjection({ minX, minY }, { maxX, maxY }, minZ * nearMultiplier, maxZ * farMultiplier);

		return ortho.getProjectionsMatrix() * lightView;
	}
}
