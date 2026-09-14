#include "CascadeShadowComponent.h"

#include <algorithm>
#include <cmath>

#include "CameraComponent.h"
#include "TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "mathModule/MatrixOperations.h"
#include "mathModule/Utils.h"
#include "rhi/GraphicsBackend.h"
#include "systemsModule/systems/CameraSystem.h"
#include "windowsModule/Window.h"


namespace SFE::ComponentsModule {
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

	void CascadeShadowComponent::calculateLightSpaceMatrices(const MathModule::PerspectiveProjection& projection, const Math::Mat4& view) {
		updateCascades(projection);
		updateLightSpaceMatrices(view);
	}

	const std::vector<Math::Mat4>& CascadeShadowComponent::getLightSpaceMatrices() const {
		return mLightSpaceMatrices;
	}

	void CascadeShadowComponent::updateCascades(const MathModule::PerspectiveProjection& cameraProjection) {
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
		// The last level is how far shadows reach, and it is authored. It used to be overwritten with
		// 70% of the camera far plane, which ties shadow quality to draw distance: at far = 5000 the
		// outer cascade stretched to 3500 units, so one texel of a 4096 map covered about a metre and
		// distant shadows turned into steps. Draw distance and shadow distance are separate choices.
		const float camFar = Engine::instance()->getWindow()->getScreenData().far;
		const float authoredFar = shadowCascadeLevels.back();
		const float shadowFar = (std::min)(authoredFar > shadowCascadeLevels.front() ? authoredFar : camFar, camFar);
		shadowCascadeLevels.back() = shadowFar;

		const float nearZ = (std::max)(shadowCascadeLevels.front(), 1e-3f);
		const size_t cascadeCount = shadowCascadeLevels.size() - 1;
		if (cascadeCount >= 2) {
			constexpr float lambda = 0.9f;
			for (size_t i = 1; i < cascadeCount; ++i) {
				const float t = static_cast<float>(i) / static_cast<float>(cascadeCount);
				const float logS = nearZ * std::pow(shadowFar / nearZ, t);
				const float uniS = nearZ + (shadowFar - nearZ) * t;
				shadowCascadeLevels[i] = logS * lambda + uniS * (1.f - lambda);
			}
		}

		auto fov = cameraProjection.getFOV();
		auto aspect = cameraProjection.getAspect();

		cascades.resize(shadowCascadeLevels.size() - 1);

		const float cascadeNear = shadowCascadeLevels.front();
		for (size_t i = 1; i < shadowCascadeLevels.size(); ++i) {
			cascades[i - 1].viewProjection = {
				fov, aspect, cascadeNear, shadowCascadeLevels[i]
			};
		}

	}

	void CascadeShadowComponent::updateLightSpaceMatrices(const Math::Mat4& cameraView) {
		mLightSpaceMatrices.clear();
		if (cascades.empty()) {
			return;
		}

		auto tc = ECSHandler::registry().pinComponent<const TransformComponent>(getEntityId());
		if (!tc) {
			return;
		}

		const auto s = Math::normalize(tc->getRight());
		const auto u = Math::normalize(tc->getUp());
		const auto f = Math::normalize(tc->getForward());

		for (auto& shadowCascade : cascades) {
			const auto fitPts = getFrustumCornersWorldSpace(
				shadowCascade.viewProjection.getProjectionsMatrix(), cameraView);

			Math::Vec3 center(0.f);
			for (const auto& p : fitPts) {
				center.x += p.x;
				center.y += p.y;
				center.z += p.z;
			}
			const float invN = 1.f / static_cast<float>(fitPts.size());
			center.x *= invN;
			center.y *= invN;
			center.z *= invN;

			const Math::Vec3 eye = center - f;
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

			Math::Mat4 cullingMatrix;
			auto projViewMatrix = getLightSpaceMatrix(fitPts, lightView, shadowCascade.zMult.x, shadowCascade.zMult.y, resolution.x, &cullingMatrix, &shadowCascade.depthRange, &shadowCascade.worldPerTexel);
			mLightSpaceMatrices.push_back(projViewMatrix);
			shadowCascade.frustum = SFE::FrustumModule::createFrustum(cullingMatrix);
		}
	}

	SFE::Math::Mat4 CascadeShadowComponent::getLightSpaceMatrix(const std::vector<SFE::Math::Vec4>& corners, const SFE::Math::Mat4& lightView, float nearMultiplier, float farMultiplier, float mapSize, SFE::Math::Mat4* cullingMatrix, float* depthRange, float* worldPerTexelOut) {
		Math::Vec4 transform = lightView * corners[0];
		auto minX = transform.x;
		auto maxX = transform.x;

		auto minY = transform.y;
		auto maxY = transform.y;

		auto minZ = transform.z;
		auto maxZ = transform.z;

		for (auto i = 1u; i < corners.size(); i++) {
			transform = lightView * corners[i];

			minX = (std::min)(minX, transform.x);
			maxX = (std::max)(maxX, transform.x);

			minY = (std::min)(minY, transform.y);
			maxY = (std::max)(maxY, transform.y);

			minZ = (std::min)(minZ, transform.z);
			maxZ = (std::max)(maxZ, transform.z);
		}

		const float cx = (minX + maxX) * 0.5f;
		const float cy = (minY + maxY) * 0.5f;
		float radius = 0.f;
		for (const auto& corner : corners) {
			transform = lightView * corner;
			const float dx = transform.x - cx;
			const float dy = transform.y - cy;
			radius = (std::max)(radius, std::sqrt(dx * dx + dy * dy));
		}

		const float res = mapSize > 1.f ? mapSize : 1024.f;
		float worldPerTexel = (radius * 2.f) / res;
		if (worldPerTexel < 1e-6f) {
			worldPerTexel = 1e-6f;
		}
		radius = std::ceil(radius / worldPerTexel) * worldPerTexel;
		radius *= 1.15f;

		minX = cx - radius;
		maxX = cx + radius;
		minY = cy - radius;
		maxY = cy + radius;

		minX = std::floor(minX / worldPerTexel) * worldPerTexel;
		minY = std::floor(minY / worldPerTexel) * worldPerTexel;
		maxX = minX + worldPerTexel * res;
		maxY = minY + worldPerTexel * res;

		if (minZ > maxZ) {
			std::swap(minZ, maxZ);
		}
		const float zRange = (std::max)(maxZ - minZ, 1.f);
		const float zNearPad = zRange * (std::clamp)(nearMultiplier, 0.5f, 3.f);
		const float zFarPad = zRange * (std::clamp)(farMultiplier, 0.5f, 3.f) * 0.25f;
		minZ -= zNearPad;
		maxZ += zFarPad;

		if (depthRange) {
			*depthRange = (std::max)(maxZ - minZ, 1e-4f);
		}
		if (worldPerTexelOut) {
			*worldPerTexelOut = worldPerTexel;
		}

		// OpenGL clip Z [-1, 1]. Vulkan RHI matches it via VK_EXT_depth_clip_control.
		const auto ortho = Math::orthoRH_NO(minX, maxX, minY, maxY, minZ, maxZ);
		if (cullingMatrix) {
			*cullingMatrix = ortho * lightView;
		}
		return ortho * lightView;
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

	void CascadeShadowComponent::debugDraw(const std::vector<SFE::Math::Mat4>& lightSpaceMatrices, const Math::Mat4& cameraProjection, const Math::Mat4& cameraView) {
		/*if (lightSpaceMatrices.empty()) {
			return;
		}

		if (!mInited) {
			mInited = true;
			Render::GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({Render::ShaderStage::Vertex, "CascadeShadowsDebug_VS", ShaderModule::ShaderBase::loadShaderCode("shaders/debugCascadeShader.vs")});
			pipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({ Render::ShaderStage::Fragment, "CascadeShadowsDebug_FS", ShaderModule::ShaderBase::loadShaderCode("shaders/debugCascadeShader.fs") });
			pipelineDesc.depthStencilState.depthTest = true;
			pipelineDesc.depthStencilState.depthWrite = true;
			pipelineDesc.depthStencilState.depthCompareOp = Render::CompareOp::Less;
			pipelineDesc.cullMode = Render::CullMode::Back;
			pipelineDesc.frontFace = Render::FrontFace::CounterClockWise;
			pipelineDesc.fillMode = Render::PolygonFillMode::Fill;
			pipelineDesc.topology = Render::PrimitiveTopology::Triangles;
			pipelineDesc.blend.enabled = true;
			pipelineDesc.blend.srcColor = Render::BlendFactor::SrcAlpha;
			pipelineDesc.blend.dstColor = Render::BlendFactor::OneMinusSrcAlpha;
			pipelineDesc.layout.bindings = {
				{0, sizeof(Math::Vec4), Render::VertexInputRate::PerVertex}
			};
			pipelineDesc.layout.attributes = {
				{0, 0, Render::VertexFormat::Float3, 0, true}
			};
			debugPipeline = Engine::instance()->rhi()->createGraphicsPipeline(pipelineDesc);

			Render::BufferDesc vertexBufferDesc;
			vertexBufferDesc.size = sizeof(Math::Vec4) * 8;
			vertexBufferDesc.usage = Render::BufferUsage::Vertex;
			vertexBufferDesc.cpuAccess = Render::CpuAccess::Write;
			debugMatricesUBO = Engine::instance()->rhi()->createBuffer(vertexBufferDesc);
			const GLuint indices[] = {
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

			Render::BufferDesc indexBufferDesc;
			indexBufferDesc.size = sizeof(GLuint) * 36;
			indexBufferDesc.usage = Render::BufferUsage::Index;
			indexBufferDesc.cpuAccess = Render::CpuAccess::Write;
			debugMatricesEBO = Engine::instance()->rhi()->createBuffer(indexBufferDesc, indices);
		}

		Engine::instance()->rhi()->bindPipeline(debugPipeline);
		Engine::instance()->rhi()->setUniform(debugPipeline, "projection", cameraProjection);
		Engine::instance()->rhi()->setUniform(debugPipeline, "view", cameraView);
		

		drawCascadeVolumeVisualizers(lightSpaceMatrices);*/
	}

	void CascadeShadowComponent::drawCascadeVolumeVisualizers(const std::vector<SFE::Math::Mat4>& lightMatrices) {
		// todo this code should not exist, move to debug renderer later
		/*static const Math::Vec4 colors[] = {
			{1.0, 0.0, 0.0, 0.5f},
			{0.0, 1.0, 0.0, 0.5f},
			{0.0, 0.0, 1.0, 0.5f},
		};

		for (int i = 0; i < lightMatrices.size(); ++i) {
			const auto corners = CascadeShadowComponent::getFrustumCornersWorldSpace(lightMatrices[i]);
			Engine::instance()->rhi()->updateBuffer(debugMatricesUBO, 0, sizeof(Math::Vec4) * corners.size(), corners.data());
			Engine::instance()->rhi()->setUniform(debugPipeline, "color", colors[i % 3]);
			Engine::instance()->rhi()->bindVertexBuffer(0, debugMatricesUBO);
			Engine::instance()->rhi()->bindIndexBuffer(debugMatricesEBO, Render::IndexType::Uint32);
			Engine::instance()->rhi()->drawIndexed(36, 0, 1, Render::IndexType::Uint32, 0);
		}*/
	}
}
