#include "PointLightPass.h"

#include <shared_mutex>

#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"

#include "logsModule/logger.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

namespace SFE::Render::RenderPasses {
	void PointLightPass::init(SystemsModule::RenderData& renderDataHandle) {
		lightProjection = SFE::MathModule::PerspectiveProjection(90.f, 1.f, 0.01f, 100);
		freeBuffers();
		TextureDesc texDesc;
		texDesc.width = shadowResolution;
		texDesc.height = shadowResolution;
		texDesc.depth = maxShadowFaces;
		texDesc.dimension = TextureDimension::Array;
		texDesc.format = TextureFormat::DEPTH_COMPONENT32F;
		texDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;
		mLightDepthMaps = Engine::instance()->rhi()->createTexture(texDesc);

		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Linear;
		samplerDesc.magFilter = FilterMode::Linear;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		samplerDesc.compareOp = CompareOp::Less;
		mLightDepthMapsSampler = Engine::instance()->rhi()->createSampler(samplerDesc);

		FramebufferDesc fbDesc;
		fbDesc.width = shadowResolution;
		fbDesc.height = shadowResolution;
		fbDesc.depthStencilAttachment = FramebufferAttachmentDesc{ mLightDepthMaps };

		lightFramebuffer = Engine::instance()->rhi()->createFramebuffer(fbDesc);

		BufferDesc bufDesc;
		bufDesc.size = sizeof(Math::Mat4) * maxShadowFaces;
		bufDesc.usage = BufferUsage::Uniform;
		bufDesc.cpuAccess = CpuAccess::Write;
		bufDesc.debugName = "PointLightPass_MatricesUBO";
		mMatricesUBO = Engine::instance()->rhi()->createBuffer(bufDesc);
		

		RenderPassDesc rpDesc;
		rpDesc.hasDepthStencil = true;
		rpDesc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
		rpDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
		renderPass = Engine::instance()->rhi()->createRenderPass(rpDesc);

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "point light vs", ShaderModule::ShaderBase::loadShaderCode("shaders/cascadeShadowMap.vs") });
		pipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "point light fs", ShaderModule::ShaderBase::loadShaderCode("shaders/cascadeShadowMap.fs") });
		pipelineDesc.shaders.geometry = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "point light gs", ShaderModule::ShaderBase::loadShaderCode("shaders/pointLightMap.gs")});
		pipelineDesc.depthStencilState.depthTest = true;
		pipelineDesc.depthStencilState.depthWrite = true;
		pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
		pipelineDesc.renderPass = renderPass;
		pipeline = Engine::instance()->rhi()->createGraphicsPipeline(pipelineDesc);

		Render::DescriptorSetLayoutDesc texLayoutDesc;
		texLayoutDesc.bindings.emplace_back(30, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Geometry);
		texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Geometry);

		mTexSetLayout = Engine::instance()->rhi()->createDescriptorSetLayout(texLayoutDesc);
		mTexSet = Engine::instance()->rhi()->allocateDescriptorSet(mTexSetLayout);
	}

	void PointLightPass::freeBuffers() const {
	}

	void PointLightPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		FUNCTION_BENCHMARK
		lightMatrices.clear();
		frustums.clear();
		renderDataHandle.mPointPassData = &data;
		data.shadowEntities.clear();

		offsets.clear();
		
		for (const auto& [entity,lightSource, transform] : ECSHandler::registry().view<LightSourceComponent, TransformComponent>()) {
			//todo check is light side frustum in camera frustum, and filter it to ignore light sources which is not on your screen
			if (!FrustumModule::SquareAABB::isOnFrustum(renderDataHandle.mCamFrustum, transform->getPos(true), lightSource->mRadius)) {
				continue;
			}

			//todo some dirty logic
			switch (lightSource->getType()) {
			case ComponentsModule::eLightType::DIRECTIONAL: {
				/*float shadowHeight = 100.f;
				float shadowWidth = 100.f;
				SFE::MathModule::OrthoProjection proj = SFE::MathModule::OrthoProjection({ -shadowWidth * 0.5f, -shadowHeight * 0.5f }, { shadowWidth * 0.5f, shadowHeight * 0.5f }, 0.01f, lightSource.mRadius);

				lightMatrices.push_back(proj.getProjectionsMatrix() * owner->getComponent<TransformComponent>()->getViewMatrix());
				frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));*/
				break;
			}
			case ComponentsModule::eLightType::POINT: {
				data.shadowEntities.push_back(entity);
				offsets.emplace_back(entity, lightSource->getTypeOffset(lightSource->getType()));
				
				fillMatrix(transform->getPos(true), lightSource->mNear, lightSource->mRadius);
				break;
			}
			case ComponentsModule::eLightType::PERSPECTIVE: {
				//lightProjection.setFar(lightSource.mRadius);//here possible some fov settings
				//lightMatrices.push_back(lightProjection.getProjectionsMatrix() * owner->getComponent<TransformComponent>()->getViewMatrix());
				//frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));
				break;
			}
			default:;
			}
		}

		if (offsets.empty()) {
			return;
		}

		if (!lightMatrices.empty()) {
			Engine::instance()->rhi()->updateBuffer(mMatricesUBO, 0, lightMatrices.size() * sizeof(Math::Mat4), lightMatrices.data());
		}

		Engine::instance()->rhi()->updateDescriptorSets({
			{
				.set = mTexSet,
				.binding = 30,
				.type = Render::DescriptorType::CombinedImageSampler,
				.images {{mLightDepthMaps, mLightDepthMapsSampler} }
			},
			{
			.set = mTexSet,
			.binding = 3,
			.type = DescriptorType::UniformBuffer,
			.buffers= {{mMatricesUBO}}
			}
		});

		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.renderPass = renderPass;
		rpBeginInfo.framebuffer = lightFramebuffer;
		rpBeginInfo.width = shadowResolution;
		rpBeginInfo.height = shadowResolution;
		Engine::instance()->rhi()->cmdBeginRenderPass(cmd, rpBeginInfo);
		Engine::instance()->rhi()->cmdBindPipeline(cmd, pipeline);
		Engine::instance()->rhi()->cmdBindDescriptorSets(cmd, pipeline, 0, &mTexSet, 1);
		
		int offsetSum = 0;
		for (auto& offset : offsets) {
			Engine::instance()->rhi()->cmdPushConstants(cmd, pipeline, offsetSum,  ShaderStage::Geometry);

			auto ocTreeSystem = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			ocTreeSystem->readLock();

	
			ecss::Ranges<ecss::EntityId> ranges; 
			for (auto& [coords, tree] : ocTreeSystem->mOctrees) {
				//auto lock = tree.readLock();
				tree.forEachObject([this, &ranges, offsetSum, &offset](const auto& obj) {
					for (auto i = offsetSum; i < offset.second + offsetSum; i++) {
						if (FrustumModule::AABB::isOnFrustum(frustums[i], obj.pos + SFE::Math::Vec3(obj.size.x, -obj.size.y, obj.size.z) * 0.5f, obj.size)){
							ranges.insert(obj.data);
							break;
						}
					}
						
					
					
				}, [this, offsetSum, &offset](const SFE::Math::Vec3& pos, float size, auto&) {
					for (auto i = offsetSum; i < offset.second + offsetSum; i++) {
						if (FrustumModule::SquareAABB::isOnFrustum(frustums[i], pos, size)) {
							return true;
						}
					}
					return false;
				});
			}

			for (const auto& [entity, mod, draw, trans  ] : ECSHandler::registry().view<const ModelComponent, const IsDrawableComponent, const TransformComponent>(ranges)) {
				if (!trans || !mod || !draw) {
					continue;
				}

				const auto& transformMatrix = trans->getTransform();
				for (auto& mesh : mod->getModelLowestDetails().meshes) {
					//batcher.addToDrawList(mesh->getVAO(), mesh->mData.vertices.size(), mesh->mData.indices.size(), mesh->mMaterial, transformMatrix, {});
				}
			}
			
			offsetSum += offset.second;
			
			//batcher.flushAll(true);
		}

		Engine::instance()->rhi()->cmdEndRenderPass(cmd);
	}

	void PointLightPass::fillMatrix(Math::Vec3 globalLightPos, float lightNear, float lightRadius) {
		lightProjection.setNearFar(lightNear, lightRadius);

		const auto transform = Math::translate(Math::Mat4(1.0f), globalLightPos);

		for (auto angle : { 0.f, 90.f, -90.f, 180.f }) {
			//todo rotate matrix instead creating rotated copy
			lightMatrices.push_back(lightProjection.getProjectionsMatrix() * Math::inverse(Math::rotate(transform, Math::radians(angle), { 0.f,1.f,0.f })));
			frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));
		}

		for (auto angle : { 90.f, -90.f }) {
			auto res = Math::rotate(transform, Math::radians(angle), { 1.f,0.f,0.f });
			lightMatrices.push_back(lightProjection.getProjectionsMatrix() * Math::inverse(res));
			frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));
		}
	}
}
