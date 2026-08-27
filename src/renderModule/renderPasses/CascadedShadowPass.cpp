#include "CascadedShadowPass.h"

#include <iostream>

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
	curPassData->mStatus = RenderPreparingStatus::PREPARING;
	auto& renderData = ECSHandler::getSystem<SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, camProj = renderData.nextCameraProjection, view = renderData.next.view]() mutable {
		FUNCTION_BENCHMARK;

		curPassData->getBatcher().clear();
		auto shadowsComp = ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource);
		if (!shadowsComp) {
			curPassData->mStatus = RenderPreparingStatus::READY;
			return;
		}

		shadowsComp->calculateLightSpaceMatrices(camProj, view);

		{
			FUNCTION_BENCHMARK_NAMED(addedToBatcher)
			auto& batcher = curPassData->getBatcher();
			auto& drawReg = ECSHandler::drawRegistry(nextRegistry);
			for (auto [ent, transform, meshComp, matComp, armatComp] : drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent, const ComponentsModule::ArmatureBonesComponent>()) {
				if (!meshComp) {
					continue;
				}
				for (const auto& mesh : meshComp->meshGraph) {
					if (!mesh.value.vertexBuffer) {
						continue;
					}
					batcher.addToDrawList(mesh.value, {}, ent);
				}
			}

			batcher.sort({100.f,1300.f, 600.f});//todo
		}
		curPassData->mStatus = RenderPreparingStatus::READY;
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
	ECSHandler::registry().addComponent<LightSourceComponent>(mShadowSource, mShadowSource, SFE::ComponentsModule::eLightType::WORLD);

	auto cmp = ECSHandler::registry().addComponent<CascadeShadowComponent>(mShadowSource, mShadowSource);
	

	auto debugData = ECSHandler::registry().addComponent<DebugDataComponent>(mShadowSource);
	debugData->stringId = "cascadeShadows";

	auto shadow = FileSystem::readJson("cascadedShadows.json");

	PropertiesModule::PropertiesSystem::deserializeProperty<CascadeShadowComponent>(mShadowSource, shadow["Properties"]);
	//cmp->resolution = Math::Vec2{ 256.f, 256.f };
	//cmp->shadowCascadeLevels = { cameraProjection.getNear(), 50.f, 150.f, /*500.f, 5000.f*/ };
	//cmp->updateCascades(cameraProjection);
	transform->setRotate({ -0.4f * 180.f,0.f, 0.4f * 5.f });

	initRender();
	renderDataHandle.mCascadedShadowsPassData = &mData;
}

void CascadedShadowPass::initRender() {
	auto cmp = ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource);
	auto& cameraProjection = ECSHandler::registry().pinComponent<const CameraComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection();

	cmp->updateCascades(cameraProjection);
	TextureDesc depthTex;
	depthTex.width = static_cast<int>(cmp->resolution.x);
	depthTex.height = static_cast<int>(cmp->resolution.y);
	depthTex.depth = static_cast<int>(cmp->cascades.size());
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

	lightDepthMa = Engine::instance()->rhi()->createTexture(depthTex);
	lightSampler = Engine::instance()->rhi()->createSampler(shadowSamp);

	lightFbos.resize(cmp->cascades.size());
	for (uint32_t c = 0; c < cmp->cascades.size(); ++c) {
		FramebufferDesc fb{};
		fb.width = static_cast<uint32_t>(cmp->resolution.x);
		fb.height = static_cast<uint32_t>(cmp->resolution.y);

		FramebufferAttachmentDesc depthAtt;
		depthAtt.texture = lightDepthMa;
		depthAtt.mipLevel = 0;
		depthAtt.layer = c;
		fb.depthStencilAttachment = depthAtt;

		lightFbos[c] = Engine::instance()->rhi()->createFramebuffer(fb);
	}
	
	BufferDesc desc;
	desc.size = sizeof(Math::Mat4) * 6; //max 6 cascades
	desc.usage = BufferUsage::Uniform;
	desc.cpuAccess = CpuAccess::Write;
	desc.debugName = "CascadedShadowPass_MatricesUBO";
	matricesUBO = Engine::instance()->rhi()->createBuffer(desc);
	mData.lightSpaceMatricesUBO = matricesUBO;

	cascadeMatrixUBOs.resize(cmp->cascades.size());
	for (size_t c = 0; c < cascadeMatrixUBOs.size(); ++c) {
		BufferDesc cascadeDesc;
		cascadeDesc.size = sizeof(Math::Mat4);
		cascadeDesc.usage = BufferUsage::Uniform;
		cascadeDesc.cpuAccess = CpuAccess::Write;
		cascadeDesc.debugName = "CascadedShadowPass_CascadeMatrixUBO";
		cascadeMatrixUBOs[c] = Engine::instance()->rhi()->createBuffer(cascadeDesc);
	}

	ShaderModuleDesc shaderVDesc;
	shaderVDesc.stage = ShaderStage::Vertex;
	shaderVDesc.name = "CascadedShadowPass_Vertex";
	shaderVDesc.source = ShaderModule::ShaderBase::loadShaderCode("shaders/cascadeShadowMap.vs");

	ShaderModuleDesc shaderFDesc;
	shaderFDesc.stage = ShaderStage::Fragment;
	shaderFDesc.name = "CascadedShadowPass_Fragment";
	shaderFDesc.source = ShaderModule::ShaderBase::loadShaderCode("shaders/cascadeShadowMap.fs");

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = true;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule(shaderVDesc);
	pipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule(shaderFDesc);
	
	Render::VertexLayoutDesc vaoDesc;
	vaoDesc.bindings.push_back({ 0,sizeof(Vertex3D), Render::VertexInputRate::PerVertex });
	vaoDesc.attributes.push_back({ 0,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), true });
	vaoDesc.attributes.push_back({ 1,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, normal)),  true });
	vaoDesc.attributes.push_back({ 2,0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords)), true });
	vaoDesc.attributes.push_back({ 3,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, tangent)), true });
	vaoDesc.attributes.push_back({ 4,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, biTangent)), true });
	vaoDesc.attributes.push_back({ 5,0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(Vertex3D, boneIDs)), false }); //todo only for dynamic mesh
	vaoDesc.attributes.push_back({ 6,0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(Vertex3D, weights)), false });

	pipelineDesc.layout = vaoDesc;

	pipeline = Engine::instance()->rhi()->createGraphicsPipeline(pipelineDesc);

	RenderPassDesc rpDesc;
	rpDesc.hasDepthStencil = true;
	rpDesc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
	rpDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
	renderPass = Engine::instance()->rhi()->createRenderPass(rpDesc);

	Render::DescriptorSetLayoutDesc shadowsLayoutDesc;
	shadowsLayoutDesc.bindings.emplace_back(0, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Vertex);
	shadowsLayoutDesc.bindings.emplace_back(4, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Vertex);

	shadowsLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	shadowsLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	shadowsLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);

	shadowsLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	shadowsLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	shadowsLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);

	shadowsLayoutHandle = Engine::instance()->rhi()->createDescriptorSetLayout(shadowsLayoutDesc);
	shadowsSetDescHandle = Engine::instance()->rhi()->allocateDescriptorSet(shadowsLayoutHandle);

	Engine::instance()->rhi()->updateDescriptorSets({
		{
			.set = shadowsSetDescHandle,
			.binding = 0,
			.type = Render::DescriptorType::UniformBuffer,
			.buffers{ {matricesUBO} }
		},
		{
			.set = shadowsSetDescHandle,
			.binding = 4,
			.type = Render::DescriptorType::UniformBuffer,
			.buffers{ {cascadeMatrixUBOs.empty() ? BufferHandle{} : cascadeMatrixUBOs[0]} }
		}
	});
}

void CascadedShadowPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mInited) {
		return;
	}
	
	debug(renderDataHandle);

	FUNCTION_BENCHMARK;

	updateRenderData(renderDataHandle);
	{
		FUNCTION_BENCHMARK_NAMED(_wait_lock);
		const auto curPassData = getContainer().getCurrentPassData();
		while(curPassData->mStatus != RenderPreparingStatus::READY) {}
	}

	auto shadowsComp = ECSHandler::registry().pinComponent<CascadeShadowComponent>(mShadowSource);
	if (!shadowsComp) {
		return;
	}

	shadowsComp->calculateLightSpaceMatrices(renderDataHandle.cameraProjection, renderDataHandle.current.view);
	const auto lightMatrices = shadowsComp->getLightSpaceMatrices();

	const auto curPassData = getContainer().getCurrentPassData();
	getContainer().rotate();
	prepare();

	updateRenderData(renderDataHandle);

	if (!lightMatrices.empty()) {
		FUNCTION_BENCHMARK_NAMED(_bind_ubo);
		mData.lightSpaceMatrices = lightMatrices;
		Engine::instance()->rhi()->updateBuffer(matricesUBO, 0, lightMatrices.size() * sizeof(Math::Mat4), lightMatrices.data());
	}

	auto rhi = Engine::instance()->rhi();

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

	for (int i = 0; i < static_cast<int>(lightFbos.size()); ++i) {
		if (!lightFbos[i]) {
			continue;
		}

		if (i < static_cast<int>(lightMatrices.size()) && i < static_cast<int>(cascadeMatrixUBOs.size())) {
			rhi->updateBuffer(cascadeMatrixUBOs[i], 0, sizeof(Math::Mat4), &lightMatrices[i]);
			rhi->updateDescriptorSets({
				{
					.set = shadowsSetDescHandle,
					.binding = 4,
					.type = Render::DescriptorType::UniformBuffer,
					.buffers{ {cascadeMatrixUBOs[i]} }
				}
			});
		}

		RenderPassBeginInfo begin;
		begin.renderPass = renderPass;
		begin.framebuffer = lightFbos[i];
		begin.x = 0;
		begin.y = 0;
		begin.width = static_cast<int>(shadowsComp->resolution.x);
		begin.height = static_cast<int>(shadowsComp->resolution.y);

		rhi->cmdBeginRenderPass(cmd, begin);
		rhi->cmdBindPipeline(cmd, pipeline);
		curPassData->getBatcher().flushAll(cmd, shadowsSetDescHandle);
		rhi->cmdEndRenderPass(cmd);
	}

	rhi->cmdPipelineBarrier(cmd, {
		.srcStage = PipelineStage::LateFragmentTests,
		.dstStage = PipelineStage::FragmentShader,
		.srcAccess = AccessMask::DepthStencilAttachmentWrite,
		.dstAccess = AccessMask::ShaderRead
	});

	{
		auto* info = SFE::SystemsModule::RenderDebugInfo::instance();
		info->shadowMeshes = curPassData->getBatcher().drawList.size();
		info->shadowFbosTotal = lightFbos.size();
		info->shadowFbos = 0;
		for (const auto& fb : lightFbos) {
			if (fb) {
				++info->shadowFbos;
			}
		}
		info->shadowPipeline = pipeline.id;
		info->shadowIntensity = mData.shadowsIntensity;
	}
}

void CascadedShadowPass::updateRenderData(SystemsModule::RenderData& renderDataHandle) {
	auto shadowsComp = ECSHandler::registry().pinComponent<const CascadeShadowComponent>(mShadowSource);
	renderDataHandle.mCascadedShadowsPassData = &mData;

	renderDataHandle.mCascadedShadowsPassData->shadowMapTexture = lightDepthMa;
	renderDataHandle.mCascadedShadowsPassData->shadowMapSampler = lightSampler;
	renderDataHandle.mCascadedShadowsPassData->lightDirection = ECSHandler::registry().pinComponent<const TransformComponent>(mShadowSource)->getForward();
	renderDataHandle.mCascadedShadowsPassData->lightColor = ECSHandler::registry().pinComponent<const LightSourceComponent>(mShadowSource)->getLightColor();
	renderDataHandle.mCascadedShadowsPassData->resolution = shadowsComp->resolution;
	renderDataHandle.mCascadedShadowsPassData->cameraFarPlane = shadowsComp->cascades.back().viewProjection.getFar();
	renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels = shadowsComp->shadowCascadeLevels;
	renderDataHandle.mCascadedShadowsPassData->shadows = mShadowSource;
	renderDataHandle.mCascadedShadowsPassData->shadowCascades = shadowsComp->cascades;
	renderDataHandle.mCascadedShadowsPassData->shadowsIntensity = shadowsComp->shadowIntensity;
	renderDataHandle.mCascadedShadowsPassData->lightSpaceMatricesUBO = matricesUBO;
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
