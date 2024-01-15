#include "CascadeShadowComponent.h"

#include "CameraComponent.h"
#include "TransformComponent.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "systemsModule/systems/CameraSystem.h"


namespace Engine::ComponentsModule {
	std::vector<Math::Vec4> CascadeShadowComponent::getFrustumCornersWorldSpace(const Math::Mat4& proj, const Math::Mat4& view) {
		return getFrustumCornersWorldSpace(proj * view);
	}

	std::vector<Math::Vec4> CascadeShadowComponent::getFrustumCornersWorldSpace(const Math::Mat4& projView) {
		const auto inv = Math::inverse(projView);

		std::vector<Math::Vec4> frustumCorners;
		for (unsigned int x = 0; x < 2; ++x) {
			for (unsigned int y = 0; y < 2; ++y) {
				for (unsigned int z = 0; z < 2; ++z) {
					const Math::Vec4 pt = inv * Math::Vec4(
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

	void CascadeShadowComponent::calculateLightSpaceMatrices(const ProjectionModule::PerspectiveProjection& projection, const Math::Mat4& view) {
		if (!mLightMatricesCache.empty()) {
			return;
		}

		updateCascades(projection);
		updateLightSpaceMatrices(view);
	}

	const std::vector<Math::Mat4>& CascadeShadowComponent::getLightSpaceMatrices() {
		if (!mLightMatricesCache.empty()) {
			return mLightMatricesCache;
		}
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

		auto cam = ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
		auto& cameraProjection = ECSHandler::registry().getComponent<CameraComponent>(cam)->getProjection();

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
		shadowCascadeLevels.back() = RenderModule::Renderer::drawDistance;

		auto fov = cameraProjection.getFOV();
		auto aspect = cameraProjection.getAspect();

		cascades.resize(shadowCascadeLevels.size() - 1);

		for (size_t i = 1; i < shadowCascadeLevels.size(); ++i) {
			cascades[i - 1].viewProjection = { fov, aspect, shadowCascadeLevels[i - 1], shadowCascadeLevels[i] };
		}

	}

	void CascadeShadowComponent::updateLightSpaceMatrices(const Math::Mat4& cameraView) {
		mLightSpaceMatrices.clear();

		for (auto& shadowCascade : cascades) {
			const auto corners = getFrustumCornersWorldSpace(shadowCascade.viewProjection.getProjectionsMatrix(), cameraView);

			Math::Vec4 frustumCenter = corners[0];
			for (size_t i = 1u; i < 8; i++) {
				frustumCenter += corners[i];
			}
			frustumCenter /= 8.f;

			auto tc = ECSHandler::registry().getComponent<TransformComponent>(getEntityId());

			auto s = Math::normalize(tc->getRight());
			auto u = Math::normalize(tc->getUp());
			auto f = Math::normalize(tc->getForward());

			auto eye = Math::Vec3(frustumCenter) - f;

			Math::Mat4 lightView(1);
			lightView[0][0] = s.x;
			lightView[1][0] = s.y;
			lightView[2][0] = s.z;
			lightView[0][1] = u.x;
			lightView[1][1] = u.y;
			lightView[2][1] = u.z;
			lightView[0][2] = -f.x;
			lightView[1][2] = -f.y;
			lightView[2][2] = -f.z;
			lightView[3][0] = -Math::dot(s, eye);
			lightView[3][1] = -Math::dot(u, eye);
			lightView[3][2] = Math::dot(f, eye);
			
			auto projViewMatrix = getLightSpaceMatrix(corners, lightView, shadowCascade.zMult.x, shadowCascade.zMult.y);

			mLightSpaceMatrices.push_back(projViewMatrix);

			shadowCascade.frustum = Engine::FrustumModule::createFrustum(projViewMatrix);
		}
	}

	Engine::Math::Mat4 CascadeShadowComponent::getLightSpaceMatrix(const std::vector<Engine::Math::Vec4>& corners, const Engine::Math::Mat4& lightView, float nearMultiplier, float farMultiplier) {
		Math::Vec4 transform = lightView * corners[0];
		auto minX = transform.x;
		auto maxX = transform.x;

		auto minY = transform.y;
		auto maxY = transform.y;

		auto minZ = transform.z;
		auto maxZ = transform.z;

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

	const std::vector<Math::Mat4>& CascadeShadowComponent::getCacheLightSpaceMatrices() {
		return mLightMatricesCache;
	}

	void CascadeShadowComponent::cacheMatrices() {
		mLightMatricesCache.clear();
		mLightMatricesCache = getLightSpaceMatrices();
	}

	void CascadeShadowComponent::clearCacheMatrices() {
		mLightMatricesCache.clear();
	}

	void CascadeShadowComponent::debugDraw(const std::vector<Engine::Math::Mat4>& lightSpaceMatrices, const Math::Mat4& cameraProjection, const Math::Mat4& cameraView) {
		if (lightSpaceMatrices.empty()) {
			return;
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		auto debugCascadeShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugCascadeShader.vs", "shaders/debugCascadeShader.fs");
		debugCascadeShader->use();
		debugCascadeShader->setMat4("projection", cameraProjection);
		debugCascadeShader->setMat4("view", cameraView);
		drawCascadeVolumeVisualizers(lightSpaceMatrices, debugCascadeShader);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);
	}

	void CascadeShadowComponent::drawCascadeVolumeVisualizers(const std::vector<Engine::Math::Mat4>& lightMatrices, Engine::ShaderModule::ShaderBase* shader) {
		static std::vector<unsigned> visualizerVAOs;
		static std::vector<unsigned> visualizerVBOs;
		static std::vector<unsigned> visualizerEBOs;

		visualizerVAOs.resize(8);
		visualizerEBOs.resize(8);
		visualizerVBOs.resize(8);

		static const GLuint indices[] = {
			0, 2, 3,
			0, 3, 1,
			4, 6, 2,
			4, 2, 0,
			5, 7, 6,
			5, 6, 4,
			1, 3, 7,
			1, 7, 5,
			6, 7, 3,
			6, 3, 2,
			1, 5, 4,
			0, 1, 4
		};

		static const Math::Vec4 colors[] = {
			{1.0, 0.0, 0.0, 0.5f},
			{0.0, 1.0, 0.0, 0.5f},
			{0.0, 0.0, 1.0, 0.5f},
		};

		for (int i = 0; i < lightMatrices.size(); ++i) {
			const auto corners = CascadeShadowComponent::getFrustumCornersWorldSpace(lightMatrices[i]);

			glGenVertexArrays(1, &visualizerVAOs[i]);
			glGenBuffers(1, &visualizerVBOs[i]);
			glGenBuffers(1, &visualizerEBOs[i]);

			glBindVertexArray(visualizerVAOs[i]);

			glBindBuffer(GL_ARRAY_BUFFER, visualizerVBOs[i]);
			glBufferData(GL_ARRAY_BUFFER, corners.size() * sizeof(corners[0]), &corners[0], GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, visualizerEBOs[i]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Math::Vec4), (void*)0);

			glBindVertexArray(visualizerVAOs[i]);
			shader->setVec4("color", colors[i % 3]);
			glDrawElements(GL_TRIANGLES, GLsizei(36), GL_UNSIGNED_INT, 0);

			glDeleteBuffers(1, &visualizerVBOs[i]);
			glDeleteBuffers(1, &visualizerEBOs[i]);
			glDeleteVertexArrays(1, &visualizerVAOs[i]);

			glBindVertexArray(0);
		}

		visualizerVAOs.clear();
		visualizerEBOs.clear();
		visualizerVBOs.clear();
	}
}
