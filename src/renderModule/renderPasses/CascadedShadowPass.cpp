#include "CascadedShadowPass.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"
#include "assetsModule/modelModule/BoundingVolume.h"
#include "systemsModule/systems/CameraSystem.h"

#include <thread>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/FileSystem.h"
#include "multithreading/ThreadPool.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "logsModule/logger.h"
#include "mathModule/Forward.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"
#include "logsModule/logger.h"

using namespace SFE::Render::RenderPasses;

CascadedShadowPass::CascadedShadowPass() {
}

CascadedShadowPass::~CascadedShadowPass() {

}

void CascadedShadowPass::init(SystemsModule::RenderData& renderDataHandle) {
	if (mInited) {
		return;
	}
	mInited = true;

	mShadowSource = ECSHandler::registry().takeEntity();
	
	auto transform = ECSHandler::registry().addComponent<TransformComponent>(mShadowSource, mShadowSource);
	auto worldLight = ECSHandler::registry().addComponent<LightSourceComponent>(mShadowSource, mShadowSource, SFE::ComponentsModule::eLightType::WORLD);
	worldLight->setLightColor({ 1.0f, 0.96f, 0.88f });
	worldLight->setIntensity(3.2f);

	auto cmp = ECSHandler::registry().addComponent<CascadeShadowComponent>(mShadowSource, mShadowSource);
	

	auto debugData = ECSHandler::registry().addComponent<DebugDataComponent>(mShadowSource);
	debugData->stringId = "cascadeShadows";

	auto shadow = FileSystem::readJson("cascadedShadows.json");

	PropertiesModule::PropertiesSystem::deserializeProperty<CascadeShadowComponent>(mShadowSource, shadow["Properties"]);
	//cmp->resolution = Math::Vec2{ 256.f, 256.f };
	//cmp->shadowCascadeLevels = { cameraProjection.getNear(), 50.f, 150.f, /*500.f, 5000.f*/ };
	//cmp->updateCascades(cameraProjection);
	transform->setRotate({ -32.f, 28.f, 8.f });

	initRender();
	renderDataHandle.frame.setData(FrameId::Csm, &mData);
	renderDataHandle.frame.set(FrameId::ShadowCsm, { .texture = lightDepthMa, .sampler = lightSampler });
	renderDataHandle.frame.set(FrameId::ShadowCsmDepth, { .texture = lightDepthMa, .sampler = lightDepthSampler });
}

void CascadedShadowPass::initRender() {
	auto cmp = ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource);
	auto& cameraProjection = ECSHandler::registry().pinComponent<const CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection();

	cmp->updateCascades(cameraProjection);

	{
		// ~6x8 UV sphere for glass shadow casters (render sphere is 32x48).
		constexpr int stacks = 6;
		constexpr int slices = 8;
		constexpr float pi = 3.14159265f;
		auto& verts = mGlassShadowMesh.mesh.vertices;
		auto& inds = mGlassShadowMesh.mesh.indices;
		verts.clear();
		inds.clear();
		verts.push_back({});
		verts.back().position = { 0.f, 1.f, 0.f };
		verts.back().normal = verts.back().position;
		for (int i = 1; i < stacks; ++i) {
			const float v = static_cast<float>(i) / static_cast<float>(stacks);
			const float phi = v * pi;
			const float sp = std::sin(phi);
			const float cp = std::cos(phi);
			for (int j = 0; j <= slices; ++j) {
				const float u = static_cast<float>(j) / static_cast<float>(slices);
				const float theta = u * 2.f * pi;
				SFE::Vertex3D vert{};
				vert.position = { sp * std::cos(theta), cp, sp * std::sin(theta) };
				vert.normal = vert.position;
				verts.push_back(vert);
			}
		}
		verts.push_back({});
		verts.back().position = { 0.f, -1.f, 0.f };
		verts.back().normal = verts.back().position;
		const unsigned south = static_cast<unsigned>(verts.size() - 1);
		for (int j = 0; j < slices; ++j) {
			inds.push_back(0);
			inds.push_back(1 + j);
			inds.push_back(1 + j + 1);
		}
		for (int i = 0; i < stacks - 2; ++i) {
			const int row = 1 + i * (slices + 1);
			const int next = row + (slices + 1);
			for (int j = 0; j < slices; ++j) {
				inds.push_back(row + j);
				inds.push_back(next + j);
				inds.push_back(row + j + 1);
				inds.push_back(row + j + 1);
				inds.push_back(next + j);
				inds.push_back(next + j + 1);
			}
		}
		const int lastRow = 1 + (stacks - 2) * (slices + 1);
		for (int j = 0; j < slices; ++j) {
			inds.push_back(lastRow + j);
			inds.push_back(south);
			inds.push_back(lastRow + j + 1);
		}
		mGlassShadowMeshData = SFE::MeshVaoRegistry::instance()->get(&mGlassShadowMesh.mesh);
	}

	TextureDesc depthTex;
	depthTex.width = static_cast<int>(cmp->resolution.x);
	depthTex.height = static_cast<int>(cmp->resolution.y);
	const uint32_t cascadeLayers = static_cast<uint32_t>((std::max)(size_t{1}, cmp->cascades.size()));
	depthTex.depth = cascadeLayers;
	depthTex.arrayLayers = cascadeLayers;
	depthTex.dimension = TextureDimension::Array;
	depthTex.format = TextureFormat::DEPTH_COMPONENT32;
	depthTex.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;

	SamplerDesc shadowSamp;
	shadowSamp.minFilter = FilterMode::Linear;
	shadowSamp.magFilter = FilterMode::Linear;
	shadowSamp.addressU = AddressMode::ClampToEdge;
	shadowSamp.addressV = AddressMode::ClampToEdge;
	shadowSamp.addressW = AddressMode::ClampToEdge;
	shadowSamp.compareOp = CompareOp::LessEqual;

	SamplerDesc depthSamp;
	depthSamp.minFilter = FilterMode::Nearest;
	depthSamp.magFilter = FilterMode::Nearest;
	depthSamp.addressU = AddressMode::ClampToEdge;
	depthSamp.addressV = AddressMode::ClampToEdge;
	depthSamp.addressW = AddressMode::ClampToEdge;
	depthSamp.compareOp = CompareOp::None;

	lightDepthMa = Engine::instance()->rhi()->createTexture(depthTex);
	lightSampler = Engine::instance()->rhi()->createSampler(shadowSamp);
	lightDepthSampler = Engine::instance()->rhi()->createSampler(depthSamp);

	auto* rhi = Engine::instance()->rhi();

	RenderPassDesc rpDesc;
	rpDesc.hasDepthStencil = true;
	rpDesc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
	rpDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
	rpDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
	renderPass = rhi->createRenderPass(rpDesc);

	// One framebuffer per layer, not one layered framebuffer: with the geometry shader gone the
	// layer comes from the attachment, and every cascade is its own render pass anyway now that
	// each draws its own culled list.
	lightFbos.clear();
	for (uint32_t layer = 0; layer < cascadeLayers; ++layer) {
		FramebufferDesc fb{};
		fb.width = static_cast<uint32_t>(cmp->resolution.x);
		fb.height = static_cast<uint32_t>(cmp->resolution.y);
		fb.renderPass = renderPass;
		fb.layers = 1;
		FramebufferAttachmentDesc depthAtt;
		depthAtt.texture = lightDepthMa;
		depthAtt.mipLevel = 0;
		depthAtt.layer = layer;
		fb.depthStencilAttachment = depthAtt;
		lightFbos.push_back(rhi->createFramebuffer(fb));
	}

	// Set 0: buffers, one per cascade. Set 1: the caster's mask texture, shared.
	Render::DescriptorSetLayoutDesc bufferLayoutDesc;
	bufferLayoutDesc.bindings.emplace_back(11, Render::DescriptorType::UniformBufferDynamic, Render::ShaderStage::Vertex);
	bufferLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	bufferLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	bufferLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	bufferLayoutDesc.bindings.emplace_back(7, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Fragment);
	mBufferLayout = rhi->createDescriptorSetLayout(bufferLayoutDesc);

	Render::DescriptorSetLayoutDesc textureLayoutDesc;
	textureLayoutDesc.bindings.emplace_back(4, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	mTextureLayout = rhi->createDescriptorSetLayout(textureLayoutDesc);

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = true;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
	pipelineDesc.cullMode = CullMode::Front;
	// Plain CCW, like every other pass. toVkFrontFace already inverts the winding to absorb the
	// un-flipped Vulkan viewport, so branching on the backend here compensated a second time and
	// left the two backends storing opposite faces: OpenGL the far surface, Vulkan the near one.
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.renderPass = renderPass;
	pipelineDesc.descriptorSetLayouts = { mBufferLayout, mTextureLayout };
	pipelineDesc.pushConstantSize = 16u;
	// The vertex shader applies the cascade matrix now, so it needs the push block too.
	pipelineDesc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
	// Not cascadeShadowMap.vs: that one stays world-space because PointLightPass shares it.
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/cascadeShadowMap_csm.vs");
	
	Render::VertexLayoutDesc vaoDesc;
	vaoDesc.bindings.push_back({ 0,sizeof(Vertex3D), Render::VertexInputRate::PerVertex });
	vaoDesc.attributes.push_back({ 0,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), true });
	vaoDesc.attributes.push_back({ 1,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, normal)),  true });
	vaoDesc.attributes.push_back({ 2,0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords)), true });
	vaoDesc.attributes.push_back({ 3,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, tangent)), true });
	vaoDesc.attributes.push_back({ 4,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, biTangent)), true });
	vaoDesc.attributes.push_back({ 5,0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(Vertex3D, boneIDs)), false });
	vaoDesc.attributes.push_back({ 6,0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(Vertex3D, weights)), false });
	vaoDesc.attributes.push_back({ 7,0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords1)), true });

	pipelineDesc.layout = vaoDesc;

	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/cascadeShadowMap_opaque.fs");
	pipelineOpaque = rhi->createGraphicsPipeline(pipelineDesc);

	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/cascadeShadowMap.fs");
	pipelineCutout = rhi->createGraphicsPipeline(pipelineDesc);

}

void CascadedShadowPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mInited) {
		return;
	}
	
	debug(renderDataHandle);

	FUNCTION_BENCHMARK;

	auto rhi = Engine::instance()->rhi();

	updateRenderData(renderDataHandle);

	auto shadowsComp = ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource);
	if (!shadowsComp) {
		return;
	}

	shadowsComp->calculateLightSpaceMatrices(renderDataHandle.cameraProjection, renderDataHandle.current.view);
	const auto lightMatrices = shadowsComp->getLightSpaceMatrices();

	updateRenderData(renderDataHandle);

	if (!lightMatrices.empty()) {
		mData.lightSpaceMatrices = lightMatrices;
	}
	const auto matrices = Utils::allocMatrices(mData.lightSpaceMatrices, CsmFrame::kMaxCascades);

	auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>();
	auto& scene = renderSystem->scene();
	if (!mProxyRegistered) {
		scene.setGlassShadowProxy(mGlassShadowMeshData);
		mProxyRegistered = true;
	}
	const auto materials = renderSystem->materials().upload();
	const auto* meshes = SFE::MeshVaoRegistry::instance();

	// A caster only needs the fragment shader when something can make it transparent: an alpha cutout, a
	// mask or a dithered opacity. Everything else writes depth on a pipeline with no fragment shader work,
	// which keeps early-Z for the bulk of the scene.
	const auto& groups = scene.groups(GpuScene::List::Shadow);
	mCascades.resize(lightFbos.size());

	// Every layer, not just the ones with casters. A cascade that renders no pass keeps last frame's depth
	// and, on Vulkan, never reaches SHADER_READ_ONLY, so sampling it is undefined.
	for (size_t layer = 0; layer < lightFbos.size(); ++layer) {
		if (!lightFbos[layer]) {
			continue;
		}
		auto& cascade = mCascades[layer];

		// The cascade's own volume, widened a little so casters just outside it still reach in.
		auto frustum = layer < shadowsComp->cascades.size() ? shadowsComp->cascades[layer].frustum : FrustumModule::Frustum{};
		for (auto* plane : { &frustum.leftFace, &frustum.rightFace, &frustum.topFace, &frustum.bottomFace, &frustum.nearFace, &frustum.farFace }) {
			plane->distance += 16.f;
		}		const bool visible = layer < shadowsComp->cascades.size()
			&& scene.cullFrustum(cmd, GpuScene::List::Shadow, cascade.view, frustum);

		if (visible) {
			if (!cascade.set) {
				cascade.set = rhi->allocateDescriptorSet(mBufferLayout);
			}
			if (matrices.buffer.id != cascade.matricesId) {
				cascade.matricesId = matrices.buffer.id;
				rhi->updateDescriptorSets({ { .set = cascade.set, .binding = 11, .buffers{ { matrices.buffer, 0, sizeof(Math::Mat4) * CsmFrame::kMaxCascades } } } });
			}
			const uint32_t ids[4] = { renderDataHandle.transformsSSBO.id, renderDataHandle.bonesSSBO.id, cascade.view.visible.id, materials.id };
			if (!std::equal(std::begin(ids), std::end(ids), std::begin(cascade.boundIds))) {
				std::copy(std::begin(ids), std::end(ids), std::begin(cascade.boundIds));
				rhi->updateDescriptorSets({
					{ .set = cascade.set, .binding = 1, .buffers{ { renderDataHandle.transformsSSBO } } },
					{ .set = cascade.set, .binding = 2, .buffers{ { renderDataHandle.bonesSSBO } } },
					{ .set = cascade.set, .binding = 3, .buffers{ { cascade.view.visible } } },
					{ .set = cascade.set, .binding = 7, .buffers{ { materials } } },
				});
			}
		}

		RenderPassBeginInfo begin;
		begin.renderPass = renderPass;
		begin.framebuffer = lightFbos[layer];
		begin.x = 0;
		begin.y = 0;
		begin.width = static_cast<int>(shadowsComp->resolution.x);
		begin.height = static_cast<int>(shadowsComp->resolution.y);
		rhi->cmdBeginRenderPass(cmd, begin);

		if (visible) {
			struct ShadowPush {
				int cascadeLayer = 0;
				float pad[3]{};
			} push;
			push.cascadeLayer = static_cast<int>(layer);

			PipelineHandle bound{};
			for (const auto& group : groups) {
				if (group.batchCount == 0) {
					continue;
				}
				const auto pipeline = group.cutout ? pipelineCutout : pipelineOpaque;
				if (pipeline.id != bound.id) {
					bound = pipeline;
					rhi->cmdBindPipeline(cmd, pipeline);
					// After the pipeline: OpenGL keeps the vertex and index buffers on the pipeline's VAO.
					rhi->cmdBindVertexBuffer(cmd, 0, meshes->vertexBuffer(), 0);
					rhi->cmdBindIndexBuffer(cmd, meshes->indexBuffer(), IndexType::Uint32, 0);
					rhi->cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
				}
				const DescriptorSetHandle sets[] = { cascade.set, mMaskSets.get(mTextureLayout, 4, group.maskTexture) };
				rhi->cmdBindDescriptorSets(cmd, pipeline, 0, sets, 2, &matrices.offset, 1);
				scene.draw(cmd, cascade.view, group);
			}
		}

		rhi->cmdEndRenderPass(cmd);
	}

	{
		auto* info = SFE::SystemsModule::RenderDebugInfo::instance();
		info->shadowMeshes = groups.size();
		info->shadowFbosTotal = static_cast<uint32_t>(lightFbos.size());
		info->shadowFbos = static_cast<uint32_t>(lightFbos.size());
		info->shadowPipeline = pipelineOpaque.id;
		info->shadowIntensity = mData.shadowsIntensity;
	}
}

void CascadedShadowPass::updateRenderData(SystemsModule::RenderData& renderDataHandle) {
	auto shadowsComp = ECSHandler::registry().pinComponent<const CascadeShadowComponent>(mShadowSource);

	mData.shadowMapTexture = lightDepthMa;
	mData.shadowMapSampler = lightSampler;
	mData.lightDirection = ECSHandler::registry().pinComponent<const TransformComponent>(mShadowSource)->getForward();
	mData.lightColor = ECSHandler::registry().pinComponent<const LightSourceComponent>(mShadowSource)->getLightColor()
		* renderDataHandle.graphics.sunIntensity;
	mData.resolution = shadowsComp->resolution;
	mData.cameraFarPlane = shadowsComp->cascades.back().viewProjection.getFar();
	mData.shadowCascadeLevels = shadowsComp->shadowCascadeLevels;
	mData.shadows = mShadowSource;
	mData.shadowCascades = shadowsComp->cascades;
	mData.shadowsIntensity = shadowsComp->shadowIntensity;

	renderDataHandle.frame.setData(FrameId::Csm, &mData);
	renderDataHandle.frame.set(FrameId::ShadowCsm, { .texture = lightDepthMa, .sampler = lightSampler });
	renderDataHandle.frame.set(FrameId::ShadowCsmDepth, { .texture = lightDepthMa, .sampler = lightDepthSampler });
}

void CascadedShadowPass::debug(SystemsModule::RenderData& renderDataHandle) {
	return;
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			if (ImGui::BeginMenu("Shadows debug")) {

				if (mShadowSource) {
					if (ImGui::Button("cache")) {
						ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource)->cacheMatrices();
					}

					if (ImGui::Button("clear")) {
						ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource)->clearCacheMatrices();
					}
					static float sunProgress = 0.4f;
					if (ImGui::DragFloat("sun pos", &sunProgress, 0.001f, 0.f)) {
						/*auto x = cosf(Math::radians(-sunProgress * 180.f));
						auto y = sinf(Math::radians(sunProgress * 180.f));
						auto z = sinf(Math::radians(sunProgress * 180.f));*/
						ECSHandler::registry().pinComponent<TransformComponent>(mShadowSource)->setRotate({ -sunProgress * 180.f,0.f, sunProgress * 5.f });
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();
}
