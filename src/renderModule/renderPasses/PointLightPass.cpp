#include "PointLightPass.h"

#include <algorithm>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/Vertex.h"
#include "mathModule/Projection.h"
#include "mathModule/Utils.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "multithreading/ThreadPool.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"

namespace {
	void appendFaceMatrices(std::vector<SFE::Math::Mat4>& out, SFE::Math::Vec3 pos, float nearP, float radius) {
		SFE::MathModule::PerspectiveProjection proj(90.f, 1.f, nearP, radius);
		const auto transform = SFE::Math::translate(SFE::Math::Mat4(1.0f), pos);
		for (auto angle : { 0.f, 90.f, -90.f, 180.f }) {
			out.push_back(proj.getProjectionsMatrix() * SFE::Math::inverse(SFE::Math::rotate(transform, SFE::Math::radians(angle), { 0.f, 1.f, 0.f })));
		}
		for (auto angle : { 90.f, -90.f }) {
			const auto rotated = SFE::Math::rotate(transform, SFE::Math::radians(angle), { 1.f, 0.f, 0.f });
			out.push_back(proj.getProjectionsMatrix() * SFE::Math::inverse(rotated));
		}
	}

	// Caster opacity and cutoff come from the material table.
	struct PointShadowPush {
		int lightCount = 0;
		float pad[3]{};
	};
}

namespace SFE::Render::RenderPasses {

PointLightPass::PointLightPass() {
	getContainer().init(2);
}

void PointLightPass::collectLights(const FrustumModule::Frustum& camFrustum, const Math::Vec3& cameraPos, std::vector<Math::Mat4>& matrices, std::vector<ecss::SectorId>* entities) {
	matrices.clear();
	if (entities) {
		entities->clear();
	}

	struct Candidate {
		ecss::SectorId id;
		Math::Vec3 pos;
		float nearP = 0.1f;
		float radius = 1.f;
		float dist2 = 0.f;
	};
	std::vector<Candidate> candidates;
	candidates.reserve(32);

	for (const auto& [entity, lightSource, transform] : ECSHandler::registry().view<LightSourceComponent, TransformComponent>()) {
		if (!lightSource || !transform || lightSource->getType() != ComponentsModule::eLightType::POINT) {
			continue;
		}
		const auto pos = transform->getPos(true);
		if (!FrustumModule::SquareAABB::isOnFrustum(camFrustum, pos, lightSource->mRadius)) {
			continue;
		}
		const auto d = pos - cameraPos;
		candidates.push_back({ entity, pos, lightSource->mNear, lightSource->mRadius, Math::dot(d, d) });
	}

	std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
		return a.dist2 < b.dist2;
	});
	if (candidates.size() > static_cast<size_t>(maxLights)) {
		candidates.resize(static_cast<size_t>(maxLights));
	}

	for (const auto& c : candidates) {
		if (entities) {
			entities->push_back(c.id);
		}
		appendFaceMatrices(matrices, c.pos, c.nearP, c.radius);
	}
}

void PointLightPass::prepare() {
	auto curPassData = getContainer().getCurrentPassData();
	curPassData->markPreparing();
	auto& renderData = ECSHandler::getSystem<SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, camFrustum = renderData.mNextCamFrustum, camPos = renderData.mNextCameraPos]() mutable {
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

		std::vector<Math::Mat4> matrices;
		collectLights(camFrustum, camPos, matrices, nullptr);
		if (matrices.empty()) {
			return;
		}

		std::vector<ecss::EntityId> ids;
		ids.reserve(2048);
		auto* octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
		if (octreeSys) {
			for (const auto& mat : matrices) {
				octreeSys->collectVisible(FrustumModule::createFrustum(mat), ids);
			}
			std::sort(ids.begin(), ids.end());
			ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
		}
		auto entities = SystemsModule::OcTreeSystem::makeEntityRanges(std::move(ids));

		auto addMeshes = [&](auto&& viewObjs) {
			auto& batcher = curPassData->getBatcher();
			for (auto [ent, transform, meshComp, matComp] : viewObjs) {
				if (!meshComp) {
					continue;
				}
				meshComp->forEachDrawMesh(matComp, [&](const SFE::RenderMeshData& mesh, const MaterialComponent* mat) {
					if (mat && mat->transparent) {
						return;
					}
					batcher.addToDrawList(mesh, mat, ent);
				});
			}
		};

		auto& drawReg = ECSHandler::drawRegistry(nextRegistry);
		if (entities.empty() && (!octreeSys || octreeSys->empty())) {
			addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
		}
		else {
			addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>(entities));
		}
	});
}

void PointLightPass::init(SystemsModule::RenderData&) {
	mInited = true;
}

void PointLightPass::ensureGpu(SystemsModule::RenderData&) {
	if (mGpuReady) {
		return;
	}
	mGpuReady = true;

	auto* rhi = Engine::instance()->rhi();

	TextureDesc texDesc;
	texDesc.width = shadowResolution;
	texDesc.height = shadowResolution;
	texDesc.depth = maxShadowFaces;
	texDesc.dimension = TextureDimension::Array;
	texDesc.format = TextureFormat::DEPTH_COMPONENT32;
	texDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;
	mLightDepthMaps = rhi->createTexture(texDesc);

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.magFilter = FilterMode::Linear;
	samplerDesc.addressU = AddressMode::ClampToEdge;
	samplerDesc.addressV = AddressMode::ClampToEdge;
	samplerDesc.addressW = AddressMode::ClampToEdge;
	samplerDesc.compareOp = CompareOp::LessEqual;
	mLightDepthMapsSampler = rhi->createSampler(samplerDesc);

	RenderPassDesc rpDesc;
	rpDesc.hasDepthStencil = true;
	rpDesc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
	rpDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
	rpDesc.depthAttachment.clearDepth = 1.f;
	rpDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
	renderPass = rhi->createRenderPass(rpDesc);

	FramebufferDesc fbDesc;
	fbDesc.width = shadowResolution;
	fbDesc.height = shadowResolution;
	fbDesc.renderPass = renderPass;
	FramebufferAttachmentDesc depthAtt;
	depthAtt.texture = mLightDepthMaps;
	depthAtt.mipLevel = 0;
	depthAtt.layer = UINT32_MAX;
	fbDesc.depthStencilAttachment = depthAtt;
	lightFramebuffer = rhi->createFramebuffer(fbDesc);

	BufferDesc bufDesc;
	bufDesc.size = sizeof(Math::Mat4) * maxShadowFaces;
	bufDesc.usage = BufferUsage::Uniform;
	bufDesc.cpuAccess = CpuAccess::Write;
	bufDesc.debugName = "PointLightPass_MatricesUBO";
	mMatricesUBO = rhi->createBuffer(bufDesc);

	Render::DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(11, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Geometry);
	layoutDesc.bindings.emplace_back(1, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(2, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(7, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(4, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	mSetLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSet = rhi->allocateDescriptorSet(mSetLayout);

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = true;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
	// Front-face culling stores the far side of thin casters (paws, tail) and
	// lifts the contact shadow off the ground. Sample both faces; closest depth wins.
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.renderPass = renderPass;
	pipelineDesc.descriptorSetLayouts = { mSetLayout };
	pipelineDesc.pushConstantSize = sizeof(PointShadowPush);
	pipelineDesc.pushConstantStages = ShaderStage::Geometry | ShaderStage::Fragment;
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/cascadeShadowMap.vs");
	pipelineDesc.shaders.geometry = rhi->loadShader(ShaderStage::Geometry, "shaders/pointLightMap.gs");
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/pointLightMap.fs");

	Render::VertexLayoutDesc vaoDesc;
	vaoDesc.bindings.push_back({ 0, sizeof(SFE::Vertex3D), Render::VertexInputRate::PerVertex });
	vaoDesc.attributes.push_back({ 0, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, position)), true });
	vaoDesc.attributes.push_back({ 1, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)), true });
	vaoDesc.attributes.push_back({ 2, 0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, texCoords)), true });
	vaoDesc.attributes.push_back({ 3, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, tangent)), true });
	vaoDesc.attributes.push_back({ 4, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, biTangent)), true });
	vaoDesc.attributes.push_back({ 5, 0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, boneIDs)), false });
	vaoDesc.attributes.push_back({ 6, 0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, weights)), false });
	vaoDesc.attributes.push_back({ 7, 0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, texCoords1)), true });
	pipelineDesc.layout = vaoDesc;
	pipeline = rhi->createGraphicsPipeline(pipelineDesc);

	rhi->updateDescriptorSets({
		{
			.set = mSet,
			.binding = 11,
			
			.buffers{ {mMatricesUBO} }
		}
	});
	if (auto* white = AssetsModule::TextureHandler::loadTexture("white.png")) {
		rhi->updateDescriptorSets({ {
			.set = mSet,
			.binding = 4,
			
			.images{ {white->texture, white->sampler} }
		} });
	}

	data.matricesUBO = mMatricesUBO;
}

void PointLightPass::publish(SystemsModule::RenderData& renderDataHandle) {
	data.matricesUBO = mMatricesUBO;
	renderDataHandle.frame.setData(FrameId::PointShadow, &data);
	renderDataHandle.frame.set(FrameId::ShadowPoint, { .texture = mLightDepthMaps, .sampler = mLightDepthMapsSampler });
}

void PointLightPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mInited) {
		return;
	}

	FUNCTION_BENCHMARK;

	std::vector<Math::Mat4> matrices;
	collectLights(renderDataHandle.mCamFrustum, renderDataHandle.mCameraPos, matrices, &data.shadowEntities);
	if (matrices.empty()) {
		publish(renderDataHandle);
		return;
	}

	ensureGpu(renderDataHandle);

	auto* rhi = Engine::instance()->rhi();
	RenderPassData* curPassData = getContainer().getCurrentPassData();
	curPassData->waitReady();
	getContainer().rotate();
	prepare();

	publish(renderDataHandle);

	rhi->updateBuffer(mMatricesUBO, 0, matrices.size() * sizeof(Math::Mat4), matrices.data());

	if (renderDataHandle.transformsSSBO) {
		rhi->updateDescriptorSets({
			{
				.set = mSet,
				.binding = 1,
				
				.buffers{ {renderDataHandle.transformsSSBO} }
			},
			{
				.set = mSet,
				.binding = 2,
				
				.buffers{ {renderDataHandle.bonesSSBO} }
			}
		});
	}

	RenderPassBeginInfo begin;
	begin.renderPass = renderPass;
	begin.framebuffer = lightFramebuffer;
	begin.x = 0;
	begin.y = 0;
	begin.width = shadowResolution;
	begin.height = shadowResolution;

	rhi->cmdBeginRenderPass(cmd, begin);
	rhi->cmdBindPipeline(cmd, pipeline);
	const int lightCount = static_cast<int>(data.shadowEntities.size());
	curPassData->getBatcher().flushAll(cmd, mSet, {},
		[this, lightCount](SFE::Render::CommandListHandle drawCmd, const DrawObject& obj) {
			PointShadowPush pc;
			pc.lightCount = lightCount;
			if (auto* white = AssetsModule::TextureHandler::loadTexture("white.png")) {
				auto diff = obj.alphaFromMaps ? obj.maskTexture() : obj.diffuseTexture();
				if (!diff) {
					diff = white->texture;
				}
				Engine::instance()->rhi()->updateDescriptorSets({ {
					.set = mSet,
					.binding = 4,
					.images{ {diff, white->sampler} }
				} });
			}
			Engine::instance()->rhi()->cmdPushConstants(drawCmd, pipeline, SFE::Render::ShaderStage::Geometry | SFE::Render::ShaderStage::Fragment, &pc, sizeof(pc));
		});
	rhi->cmdEndRenderPass(cmd);
}

}
