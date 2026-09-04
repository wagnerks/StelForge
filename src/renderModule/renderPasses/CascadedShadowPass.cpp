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

void CascadedShadowPass::prepare() {
	if (!ECSHandler::registry().pinComponent<const CascadeShadowComponent>(mShadowSource)) {
		return;
	}

	auto curPassData = getContainer().getCurrentPassData();
	curPassData->markPreparing();
	auto& renderData = ECSHandler::getSystem<SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, camFrustum = renderData.mNextCamFrustum]() mutable {
		FUNCTION_BENCHMARK;
		struct ReadyGuard {
			RenderPassData* data = nullptr;
			~ReadyGuard() { if (data) { data->markReady(); } }
		} ready{ curPassData };

		auto* renderSys = ECSHandler::getSystem<SystemsModule::RenderSystem>();
		auto worldLock = renderSys ? renderSys->tryLockWorldRead() : SystemsModule::RenderSystem::WorldReadGuard{};
		if (renderSys && !worldLock) {
			return;
		}

		curPassData->getBatcher().clear();

		{
			FUNCTION_BENCHMARK_NAMED(addedToBatcher)
			auto& batcher = curPassData->getBatcher();
			auto& drawReg = ECSHandler::drawRegistry(nextRegistry);

			ecss::Ranges<ecss::EntityId> entities;
			if (const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>()) {
				auto inflate = [](FrustumModule::Frustum f, float e) {
					f.leftFace.distance += e;
					f.rightFace.distance += e;
					f.topFace.distance += e;
					f.bottomFace.distance += e;
					f.nearFace.distance += e;
					f.farFace.distance += e;
					return f;
				};

				std::vector<ecss::EntityId> ids;
				ids.reserve(2048);
				// Camera frustum only — light-cascade collect was pulling near-everything.
				octreeSys->collectVisible(inflate(camFrustum, 16.f), ids);
				entities = SystemsModule::OcTreeSystem::makeEntityRanges(std::move(ids));
			}

			auto addMeshes = [&](auto&& viewObjs) {
				size_t n = 0;
				for (auto [ent, transform, meshComp, matComp] : viewObjs) {
					if (!meshComp) {
						continue;
					}
					meshComp->forEachDrawMesh(matComp, [&](const SFE::RenderMeshData& mesh, const MaterialComponent* mat) {
						if (mat && mat->transparent && mGlassShadowMeshData.vertexBuffer) {
							batcher.addToDrawList(mGlassShadowMeshData, mat, ent);
						}
						else {
							batcher.addToDrawList(mesh, mat, ent);
						}
						++n;
					});
				}
				SystemsModule::RenderDebugInfo::instance()->cullShadow = n;
				return n;
			};
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			size_t added = 0;
			if (entities.empty() && (!octreeSys || octreeSys->empty())) {
				added = addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
			}
			else {
				added = addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>(entities));
			}
			if (added == 0) {
				added = addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
			}
		}
	});
}

CascadedShadowPass::CascadedShadowPass() {
	getContainer().init(2);
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
	renderDataHandle.frame.set(FrameId::ShadowMatrices, { .buffer = matricesUBO });
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

	FramebufferDesc fb{};
	fb.width = static_cast<uint32_t>(cmp->resolution.x);
	fb.height = static_cast<uint32_t>(cmp->resolution.y);
	fb.renderPass = renderPass;
	fb.layers = cascadeLayers;
	FramebufferAttachmentDesc depthAtt;
	depthAtt.texture = lightDepthMa;
	depthAtt.mipLevel = 0;
	depthAtt.layer = UINT32_MAX;
	fb.depthStencilAttachment = depthAtt;
	lightFbo = rhi->createFramebuffer(fb);
	
	BufferDesc desc;
	desc.size = sizeof(Math::Mat4) * 6; //max 6 cascades
	desc.usage = BufferUsage::Uniform;
	desc.cpuAccess = CpuAccess::Write;
	desc.debugName = "CascadedShadowPass_MatricesUBO";
	matricesUBO = rhi->createBuffer(desc);
	mData.lightSpaceMatricesUBO = matricesUBO;

	Render::DescriptorSetLayoutDesc shadowsLayoutDesc;
	shadowsLayoutDesc.bindings.emplace_back(11, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Geometry);
	shadowsLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	shadowsLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	shadowsLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);

	shadowsLayoutHandle = rhi->createDescriptorSetLayout(shadowsLayoutDesc);
	shadowsSetDescHandle = rhi->allocateDescriptorSet(shadowsLayoutHandle);

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = true;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
	pipelineDesc.cullMode = CullMode::Front;
	// Vulkan RHI inverts winding (viewport is not Y-flipped). Light ortho is not Y-flipped
	// either, so a shared CCW frontFace would make GL write back faces and VK front faces.
	// Keep Vulkan as-is (front faces); flip GL so both write the same casters.
	pipelineDesc.frontFace = rhi->backend() == GraphicsBackend::Vulkan
		? FrontFace::CounterClockWise
		: FrontFace::ClockWise;
	pipelineDesc.renderPass = renderPass;
	pipelineDesc.descriptorSetLayouts = { shadowsLayoutHandle };
	pipelineDesc.pushConstantSize = 16u;
	pipelineDesc.pushConstantStages = ShaderStage::Fragment | ShaderStage::Geometry;
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/cascadeShadowMap.vs");
	pipelineDesc.shaders.geometry = rhi->loadShader(ShaderStage::Geometry, "shaders/cascadeShadowMap.gs");
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/cascadeShadowMap.fs");
	
	Render::VertexLayoutDesc vaoDesc;
	vaoDesc.bindings.push_back({ 0,sizeof(Vertex3D), Render::VertexInputRate::PerVertex });
	vaoDesc.attributes.push_back({ 0,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), true });
	vaoDesc.attributes.push_back({ 1,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, normal)),  true });
	vaoDesc.attributes.push_back({ 2,0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords)), true });
	vaoDesc.attributes.push_back({ 3,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, tangent)), true });
	vaoDesc.attributes.push_back({ 4,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, biTangent)), true });
	vaoDesc.attributes.push_back({ 5,0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(Vertex3D, boneIDs)), false });
	vaoDesc.attributes.push_back({ 6,0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(Vertex3D, weights)), false });

	pipelineDesc.layout = vaoDesc;

	pipeline = rhi->createGraphicsPipeline(pipelineDesc);

	rhi->updateDescriptorSets({
		{
			.set = shadowsSetDescHandle,
			.binding = 11,
			.type = Render::DescriptorType::UniformBuffer,
			.buffers{ {matricesUBO} }
		}
	});
}

void CascadedShadowPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mInited) {
		return;
	}
	
	debug(renderDataHandle);

	FUNCTION_BENCHMARK;

	auto rhi = Engine::instance()->rhi();

	updateRenderData(renderDataHandle);

	auto* curPassData = getContainer().getCurrentPassData();
	curPassData->waitReady();
	getContainer().rotate();
	prepare();

	auto shadowsComp = ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource);
	if (!shadowsComp) {
		return;
	}

	shadowsComp->calculateLightSpaceMatrices(renderDataHandle.cameraProjection, renderDataHandle.current.view);
	const auto lightMatrices = shadowsComp->getLightSpaceMatrices();

	updateRenderData(renderDataHandle);

	if (!lightMatrices.empty()) {
		FUNCTION_BENCHMARK_NAMED(_bind_ubo);
		mData.lightSpaceMatrices = lightMatrices;
		Math::Mat4 packed[6]{};
		const size_t n = (std::min)(lightMatrices.size(), size_t{6});
		for (size_t i = 0; i < 6; ++i) {
			packed[i] = lightMatrices[i < n ? i : n - 1];
		}
		rhi->updateBuffer(matricesUBO, 0, sizeof(packed), packed);
	}

	if (renderDataHandle.transformsSSBO) {
		rhi->updateDescriptorSets({
			{
				.set = shadowsSetDescHandle,
				.binding = 1,
				.type = Render::DescriptorType::StorageBuffer,
				.buffers{ {renderDataHandle.transformsSSBO} }
			},
			{
				.set = shadowsSetDescHandle,
				.binding = 2,
				.type = Render::DescriptorType::StorageBuffer,
				.buffers{ {renderDataHandle.bonesSSBO} }
			}
		});
	}

	if (lightFbo) {
		RenderPassBeginInfo begin;
		begin.renderPass = renderPass;
		begin.framebuffer = lightFbo;
		begin.x = 0;
		begin.y = 0;
		begin.width = static_cast<int>(shadowsComp->resolution.x);
		begin.height = static_cast<int>(shadowsComp->resolution.y);

		rhi->cmdBeginRenderPass(cmd, begin);
		rhi->cmdBindPipeline(cmd, pipeline);
		const int cascadeDraws = static_cast<int>(lightMatrices.size());
		for (int layer = 0; layer < cascadeDraws; ++layer) {
			curPassData->getBatcher().flushAll(cmd, shadowsSetDescHandle, {},
				[this, layer](SFE::Render::CommandListHandle drawCmd, const DrawObject& obj) {
					struct ShadowPush {
						float casterOpacity = 1.f;
						int cascadeLayer = 0;
						float pad1 = 0.f;
						float pad2 = 0.f;
					} pc;
					pc.casterOpacity = (obj.opacity > 0.f && obj.opacity < 0.999f) ? obj.opacity : 1.f;
					pc.cascadeLayer = layer;
					Engine::instance()->rhi()->cmdPushConstants(
						drawCmd, pipeline,
						SFE::Render::ShaderStage::Fragment | SFE::Render::ShaderStage::Geometry,
						&pc, sizeof(pc));
				}, nullptr, false);
		}
		rhi->cmdEndRenderPass(cmd);
	}

	{
		auto* info = SFE::SystemsModule::RenderDebugInfo::instance();
		info->shadowMeshes = curPassData->getBatcher().drawList.size();
		info->shadowFbosTotal = lightFbo ? 1u : 0u;
		info->shadowFbos = lightFbo ? 1u : 0u;
		info->shadowPipeline = pipeline.id;
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
	mData.lightSpaceMatricesUBO = matricesUBO;

	renderDataHandle.frame.setData(FrameId::Csm, &mData);
	renderDataHandle.frame.set(FrameId::ShadowCsm, { .texture = lightDepthMa, .sampler = lightSampler });
	renderDataHandle.frame.set(FrameId::ShadowCsmDepth, { .texture = lightDepthMa, .sampler = lightDepthSampler });
	renderDataHandle.frame.set(FrameId::ShadowMatrices, { .buffer = matricesUBO });
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
