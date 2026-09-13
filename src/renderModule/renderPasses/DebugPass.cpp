#include "DebugPass.h"

#include "assetsModule/shaderModule/ShaderBase.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/GizmoComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "renderModule/FrameBlackboard.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

#include <algorithm>


namespace SFE::Render::RenderPasses {
	namespace {
		BufferHandle makeDebugVBO(RenderHardwareInterface* rhi, size_t size) {
			BufferDesc desc;
			desc.size = size;
			desc.usage = BufferUsage::Vertex;
			desc.cpuAccess = CpuAccess::Write;
			return rhi->createBuffer(desc);
		}

		// cmdUpdateBuffer writes at the call, so every draw of the frame needs its own region.
		void growDebugVBO(RenderHardwareInterface* rhi, BufferHandle& vbo, size_t& capacity, size_t needed) {
			if (needed <= capacity) {
				return;
			}
			capacity = (std::max)(needed, capacity * 2);
			rhi->destroyBuffer(vbo);
			vbo = makeDebugVBO(rhi, capacity);
		}
	}

	DebugPass::DebugPass() = default;

	void DebugPass::init(SystemsModule::RenderData&) {
		auto rhi = Engine::instance()->rhi();

		RenderPassDesc rpd;
		rpd.colorAttachmentCount = 1;
		rpd.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		rpd.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		rpd.colorAttachments[0].format = TextureFormat::RGBA8;
		debugRenderPass = rhi->createRenderPass(rpd);

		mLinesCapacity = 65536 * sizeof(Math::Vec3);
		linesVBO = makeDebugVBO(rhi, mLinesCapacity);

		VertexLayoutDesc lineLayout;
		lineLayout.bindings.push_back({ 0, sizeof(Math::Vec3), VertexInputRate::PerVertex });
		lineLayout.attributes.push_back({
			0,
			0,
			VertexFormat::Float3,
			0,
			false
			});

		mTrisCapacity = 4096 * 3 * sizeof(SFE::Vertex3D);
		trianglesVBO = makeDebugVBO(rhi, mTrisCapacity);

		VertexLayoutDesc triLayout;
		triLayout.bindings.push_back({ 0, sizeof(SFE::Vertex3D), VertexInputRate::PerVertex });

		triLayout.attributes.push_back({
			0,
			0,
			VertexFormat::Float3,
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, position)),
			false
			});

		triLayout.attributes.push_back({
			1,
			0,
			VertexFormat::Float3,
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)),
			false
			});

		triLayout.attributes.push_back({
			2,
			0,
			VertexFormat::Float4,
			static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, color)),
			false
			});

		GraphicsPipelineDesc debugPipelineDesc{};
		debugPipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/simpleColoredTriangle.vs");
		debugPipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/simpleColoredTriangle.fs");
		debugPipelineDesc.topology = PrimitiveTopology::Triangles;
		debugPipelineDesc.cullMode = CullMode::None;
		debugPipelineDesc.frontFace = FrontFace::CounterClockWise;
		debugPipelineDesc.fillMode = PolygonFillMode::Fill;
		debugPipelineDesc.depthStencilState.depthTest = false;
		debugPipelineDesc.depthStencilState.depthWrite = false;
		debugPipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
		debugPipelineDesc.blend.enabled = true;
		debugPipelineDesc.blend.srcColor = BlendFactor::SrcAlpha;
		debugPipelineDesc.blend.dstColor = BlendFactor::OneMinusSrcAlpha;
		debugPipelineDesc.layout = triLayout;
		debugPipelineDesc.renderPass = debugRenderPass;
		debugPipelineDesc.pushConstantSize = sizeof(DrawDataTriangles);
		debugPipelineDesc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		trianglesPipeline = rhi->createGraphicsPipeline(debugPipelineDesc);

		GraphicsPipelineDesc linePipelineDesc;
		linePipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/xyzLines.vs");
		linePipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/colored_lines.fs");
		linePipelineDesc.layout = lineLayout;
		linePipelineDesc.topology = PrimitiveTopology::Lines;
		linePipelineDesc.cullMode = CullMode::None;
		linePipelineDesc.frontFace = FrontFace::CounterClockWise;
		linePipelineDesc.fillMode = PolygonFillMode::Fill;
		linePipelineDesc.depthStencilState.depthTest = false;
		linePipelineDesc.depthStencilState.depthWrite = false;
		linePipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
		linePipelineDesc.blend.enabled = false;
		linePipelineDesc.renderPass = debugRenderPass;
		linePipelineDesc.pushConstantSize = sizeof(DrawDataLines);
		linePipelineDesc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		linesPipeline = rhi->createGraphicsPipeline(linePipelineDesc);
	}

	DebugPass::~DebugPass() {}

	void DebugPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		FUNCTION_BENCHMARK;

		if (const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>()) {
			if (octreeSys->drawOctrees) {
				for (auto& [pos, tree] : octreeSys->mOctrees) {
					(void)pos;
					auto lock = tree.readLock();
					tree.drawOctree(octreeSys->drawObjAABB);
				}
			}
		}

		for (auto [entId, gizmoComp] : ECSHandler::registry().view<ComponentsModule::GizmoComponent>()) {
			gizmoComp->gizmo.setEntity(entId);
			gizmoComp->gizmo.update();
		}

		auto* rhi = Engine::instance()->rhi();
		auto fb = renderDataHandle.frame.framebuffer(FrameId::Final);
		auto* dbg = SystemsModule::RenderDebugInfo::instance();
		if (!fb || (Utils::renderTriangles.empty() && Utils::renderVertices.empty())) {
			Utils::renderTriangles.clear();
			Utils::renderVertices.clear();
			return;
		}

		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.renderPass = debugRenderPass;
		rpBeginInfo.framebuffer = fb;
		rpBeginInfo.width = Engine::instance()->getWindow()->getScreenData().renderW;
		rpBeginInfo.height = Engine::instance()->getWindow()->getScreenData().renderH;
		rhi->cmdBeginRenderPass(cmd, rpBeginInfo);

		if (!Utils::renderTriangles.empty() && trianglesPipeline) {
			drawDataTriangles.pv = renderDataHandle.current.PV;
			drawDataTriangles.viewPos = Math::Vec4(renderDataHandle.mCameraPos, 1.f);
			const size_t triStride = sizeof(Utils::Triangle);
			size_t totalTris = 0;
			for (const auto& [data, triangles] : Utils::renderTriangles) {
				(void)data;
				totalTris += triangles.size();
			}
			growDebugVBO(rhi, trianglesVBO, mTrisCapacity, totalTris * triStride);
			rhi->cmdBindPipeline(cmd, trianglesPipeline);
			rhi->cmdBindVertexBuffer(cmd, 0, trianglesVBO);
			rhi->cmdPushConstants(cmd, trianglesPipeline, drawDataTriangles);
			size_t firstTri = 0;
			for (auto& [data, triangles] : Utils::renderTriangles) {
				(void)data;
				if (triangles.empty()) {
					continue;
				}
				rhi->cmdUpdateBuffer(cmd, trianglesVBO, firstTri * triStride, triangles.size() * triStride, triangles.data());
				rhi->cmdDraw(cmd, static_cast<uint32_t>(triangles.size() * 3), 1, static_cast<uint32_t>(firstTri * 3));
				++dbg->drawCalls;
				firstTri += triangles.size();
			}
			Utils::renderTriangles.clear();
		}

		if (!Utils::renderVertices.empty() && linesPipeline) {
			size_t totalVertices = 0;
			for (const auto& [data, vertices] : Utils::renderVertices) {
				(void)data;
				totalVertices += vertices.size();
			}
			growDebugVBO(rhi, linesVBO, mLinesCapacity, totalVertices * sizeof(Math::Vec3));
			size_t firstVertex = 0;
			rhi->cmdBindPipeline(cmd, linesPipeline);
			rhi->cmdBindVertexBuffer(cmd, 0, linesVBO);
			drawDataLines.pv = renderDataHandle.current.PV;
			const size_t vertStride = sizeof(Math::Vec3);
			for (auto& [data, vertices] : Utils::renderVertices) {
				drawDataLines.color = data.color;
				rhi->cmdPushConstants(cmd, linesPipeline, drawDataLines);
				if (vertices.empty()) {
					continue;
				}
				rhi->cmdUpdateBuffer(cmd, linesVBO, firstVertex * vertStride, vertices.size() * vertStride, vertices.data());
				rhi->cmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, static_cast<uint32_t>(firstVertex));
				++dbg->drawCalls;
				firstVertex += vertices.size();
			}
			Utils::renderVertices.clear();
		}

		rhi->cmdEndRenderPass(cmd);
	}

	PipelineHandle DebugPass::getTrianglePipeline(bool blend, bool depthTest, bool cull, std::pair<BlendFactor, BlendFactor> blendFactors)
	{
		PipelineDesc desc;
		desc.blend = blend;
		desc.depthTest = depthTest;
		desc.cull = cull;
		desc.blendFactors = blendFactors;

		if (!trianglesPipelinesCache.contains(desc)) {
			GraphicsPipelineDesc debugPipelineDesc{};
			auto* rhi = Engine::instance()->rhi();
			debugPipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/simpleColoredTriangle.vs");
			debugPipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/simpleColoredTriangle.fs");
			debugPipelineDesc.renderPass = debugRenderPass;
			debugPipelineDesc.pushConstantSize = sizeof(DrawDataTriangles);
			debugPipelineDesc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;

			debugPipelineDesc.topology = PrimitiveTopology::Triangles;
			debugPipelineDesc.cullMode = cull ? CullMode::Back : CullMode::None;
			debugPipelineDesc.frontFace = FrontFace::CounterClockWise;
			debugPipelineDesc.fillMode = PolygonFillMode::Fill;
			debugPipelineDesc.depthStencilState.depthTest = depthTest;
			debugPipelineDesc.depthStencilState.depthWrite = depthTest;
			debugPipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
			debugPipelineDesc.blend.enabled = blend;
			if (blend) {
				debugPipelineDesc.blend.srcColor = blendFactors.first;
				debugPipelineDesc.blend.dstColor = blendFactors.second;
			}
			debugPipelineDesc.layout = VertexLayoutDesc{};
			debugPipelineDesc.layout.bindings.push_back({ 0, sizeof(SFE::Vertex3D), VertexInputRate::PerVertex });
			debugPipelineDesc.layout.attributes.push_back({
				0,                            // location 0
				0,
				VertexFormat::Float3,         // vec3 position
				static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, position)),
				false                         // normalized
				});
			debugPipelineDesc.layout.attributes.push_back({
				1,                            // location 1
				0,
				VertexFormat::Float3,         // vec3 normal
				static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)),
				true                          // normalized = true
				});
			debugPipelineDesc.layout.attributes.push_back({
				2,                            // location 2
				0,
				VertexFormat::Float4,         // vec4 color
				static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, color)),
				true                          // normalized = true
				});

			trianglesPipelinesCache[desc] = rhi->createGraphicsPipeline(debugPipelineDesc);
		}

		return trianglesPipelinesCache[desc];
	}
}

