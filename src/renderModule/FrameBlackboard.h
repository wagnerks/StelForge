#pragma once

#include "componentsModule/CascadeShadowComponent.h"
#include "ecss/Types.h"
#include "mathModule/Forward.h"
#include "renderModule/RenderHardwareInterface.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace SFE::Render {

	struct FrameResource {
		TextureHandle texture{};
		SamplerHandle sampler{};
		BufferHandle buffer{};
		FramebufferHandle framebuffer{};
	};

	struct CsmFrame {
		TextureHandle shadowMapTexture;
		SamplerHandle shadowMapSampler;
		Math::Vec3 lightDirection = {};
		Math::Vec3 lightColor = {};
		Math::Vec2 resolution = {};
		float cameraFarPlane = 0.f;
		std::vector<float> shadowCascadeLevels;
		ecss::EntityId shadows = ecss::INVALID_ID;
		std::vector<ComponentsModule::ShadowCascade> shadowCascades;
		float shadowsIntensity = 0.f;
		BufferHandle lightSpaceMatricesUBO;
		std::vector<Math::Mat4> lightSpaceMatrices;
	};

	struct PointShadowFrame {
		std::vector<ecss::SectorId> shadowEntities;
		BufferHandle matricesUBO;
	};

	class FrameBlackboard {
	public:
		void set(const char* name, FrameResource resource) {
			mGpu[name] = resource;
		}

		TextureHandle texture(const char* name) const {
			const auto* r = find(name);
			return r ? r->texture : TextureHandle{};
		}

		SamplerHandle sampler(const char* name) const {
			const auto* r = find(name);
			return r ? r->sampler : SamplerHandle{};
		}

		BufferHandle buffer(const char* name) const {
			const auto* r = find(name);
			return r ? r->buffer : BufferHandle{};
		}

		FramebufferHandle framebuffer(const char* name) const {
			const auto* r = find(name);
			return r ? r->framebuffer : FramebufferHandle{};
		}

		template<typename T>
		void setData(const char* name, T* ptr) {
			mCpu[name] = ptr;
		}

		template<typename T>
		T* data(const char* name) const {
			auto it = mCpu.find(name);
			if (it == mCpu.end()) {
				return nullptr;
			}
			return static_cast<T*>(it->second);
		}

	private:
		const FrameResource* find(const char* name) const {
			auto it = mGpu.find(name);
			return it == mGpu.end() ? nullptr : &it->second;
		}

		std::unordered_map<std::string, FrameResource> mGpu;
		std::unordered_map<std::string, void*> mCpu;
	};

	namespace FrameId {
		constexpr const char* GBufferFbo = "gbuffer.fbo";
		constexpr const char* GBufferOutlineFbo = "gbuffer.outlineFbo";
		constexpr const char* GBufferPosition = "gbuffer.position";
		constexpr const char* GBufferNormal = "gbuffer.normal";
		constexpr const char* GBufferAlbedo = "gbuffer.albedo";
		constexpr const char* GBufferViewPosition = "gbuffer.viewPosition";
		constexpr const char* GBufferOutlines = "gbuffer.outlines";
		constexpr const char* GBufferLights = "gbuffer.lights";
		constexpr const char* GBufferDepth = "gbuffer.depth";
		constexpr const char* ShadowCsm = "shadow.csm";
		constexpr const char* ShadowCsmDepth = "shadow.csmDepth";
		constexpr const char* ShadowMatrices = "shadow.matrices";
		constexpr const char* SsaoColor = "ssao.color";
		constexpr const char* SsaoBlur = "ssao.blur";
		constexpr const char* Lit = "lighting.color";
		constexpr const char* IblIrradiance = "ibl.irradiance";
		constexpr const char* IblPrefilter = "ibl.prefilter";
		constexpr const char* IblBrdf = "ibl.brdf";
		constexpr const char* Final = "final";
		constexpr const char* Csm = "csm";
		constexpr const char* PointShadow = "pointShadow";
		constexpr const char* ShadowPoint = "shadow.point";
	}

}
