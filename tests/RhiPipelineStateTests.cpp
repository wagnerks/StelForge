#include <optional>

#include "RhiTestFixture.h"

// Pipeline state: the fixed-function knobs a GraphicsPipelineDesc carries. Unlike winding or the
// depth range, most of these produce a number rather than a yes/no — a blend is arithmetic on the
// value already in the attachment — so "both backends support blending" is not the same claim as
// "both backends hand back the same bytes". These tests pin the bytes.
namespace SFE::Render::Tests {
	namespace {

		constexpr int kSize = 8;

		struct TrianglePush {
			Math::Vec4 a{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 b{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 c{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f };
		};

		TextureHandle makeTarget(RenderHardwareInterface& rhi) {
			TextureDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.depth = 1;
			desc.arrayLayers = 1;
			desc.dimension = TextureDimension::Tex2D;
			desc.format = TextureFormat::RGBA8;
			desc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
			return rhi.createTexture(desc);
		}

		TextureHandle makeDepthTarget(RenderHardwareInterface& rhi) {
			TextureDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.depth = 1;
			desc.arrayLayers = 1;
			desc.dimension = TextureDimension::Tex2D;
			desc.format = TextureFormat::DEPTH_COMPONENT32;
			desc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
			return rhi.createTexture(desc);
		}

		// Colour and depth in one pass, both cleared on begin. Depth clears to the far value so the
		// first draw always lands whatever compare op it carries, and re-clears per case, which is
		// what lets one pass object serve every row of the compare-op table.
		RenderPassHandle makeColorDepthPass(RenderHardwareInterface& rhi, const Math::Vec4& clear, float clearDepth) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = clear;
			desc.hasDepthStencil = true;
			desc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
			desc.depthAttachment.storeOp = AttachmentStoreOp::Store;
			desc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
			desc.depthAttachment.clearDepth = clearDepth;
			return rhi.createRenderPass(desc);
		}

		// The state under test, everything else at the value the rest of the suite has already
		// pinned: no culling (winding is RhiConventionTests' business), no depth by default (a
		// colour-only pass carries no depth attachment), filled triangles.
		struct PipelineOptions {
			BlendStateDesc blend{};
			DepthStencilStateDesc depthStencil{ false, false, CompareOp::Always };
			PrimitiveTopology topology = PrimitiveTopology::Triangles;
			PolygonFillMode fill = PolygonFillMode::Fill;
			CullMode cull = CullMode::None;
		};

		PipelineHandle makePipeline(RenderHardwareInterface& rhi, RenderPassHandle pass, const PipelineOptions& options) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = options.topology;
			desc.cullMode = options.cull;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = options.fill;
			desc.depthStencilState = options.depthStencil;
			desc.blend = options.blend;
			desc.pushConstantSize = sizeof(TrianglePush);
			desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		// Position and colour in one buffer, the same record RhiDrawTests uses with the same pair of
		// shaders. Needed because rhi_test_triangle.vs builds its three corners out of the vertex
		// index and the push block: any draw of more than three vertices has to come from a buffer.
		struct Vertex {
			float x = 0.f;
			float y = 0.f;
			float r = 0.f;
			float g = 0.f;
			float b = 0.f;
			float a = 1.f;
		};

		// The same pipeline as makePipeline but reading its vertices from a buffer, with the topology
		// under test. Culling stays off and the depth state stays inert, so nothing but the grouping of
		// the vertices can decide which pixels are painted.
		PipelineHandle makeVertexPipeline(RenderHardwareInterface& rhi, RenderPassHandle pass, PrimitiveTopology topology) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = topology;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.layout.bindings.push_back({ 0, sizeof(Vertex), VertexInputRate::PerVertex });
			desc.layout.attributes.push_back({ 0, 0, VertexFormat::Float2, offsetof(Vertex, x), false });
			desc.layout.attributes.push_back({ 1, 0, VertexFormat::Float4, offsetof(Vertex, r), false });
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_vertex.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_vertex.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		BufferHandle makeVertexBuffer(RenderHardwareInterface& rhi, const Vertex* data, size_t count) {
			BufferDesc desc;
			desc.size = count * sizeof(Vertex);
			desc.usage = BufferUsage::Vertex;
			desc.cpuAccess = CpuAccess::Write;
			desc.debugName = "rhi_test_topology_vertex_buffer";
			return rhi.createBuffer(desc, data);
		}

		// Clip-space coordinate of a pixel centre on the kSize target. A vertex put here sits half a
		// pixel clear of every pixel boundary, so a point lands unambiguously in one pixel and a line
		// through two of them runs along a row or column of centres instead of between them.
		constexpr float pixelCentreX(int col) { return (static_cast<float>(col) + 0.5f) / 4.f - 1.f; }
		constexpr float pixelCentreY(int row) { return 1.f - (static_cast<float>(row) + 0.5f) / 4.f; }

		// A rectangle in whatever coordinates cmdSetScissor takes. The field names match the call's
		// parameters, so a test reads as the call it makes and nothing here presumes an origin corner.
		struct ScissorRect {
			int32_t x = 0;
			int32_t y = 0;
			uint32_t width = 0;
			uint32_t height = 0;
		};

		struct DrawStep {
			PipelineHandle pipeline;
			TrianglePush push;
			// Set per step rather than per pass: a step without one draws with whatever scissor state
			// the pass began with, which is the "no cmdSetScissor at all" case pinned below.
			std::optional<ScissorRect> scissor;
			// Set to draw from a vertex buffer instead of from the push block. The two are exclusive:
			// a pipeline built by makeVertexPipeline declares no push constants, so a step with a
			// buffer must not be handed one.
			BufferHandle vertices;
			uint32_t vertexCount = 3;
		};

		// Clears once, then replays every step into that same render pass. Keeping both draws inside
		// one pass is what makes the second one blend against the first: an AttachmentLoadOp::Load
		// pass would work too, but it would drag a second pass object and the load-op contract into a
		// test about blend factors, and load-op preservation is its own checkbox elsewhere.
		std::vector<uint8_t> drawSteps(RenderHardwareInterface& rhi, TextureHandle target,
			RenderPassHandle pass, const std::vector<DrawStep>& steps, TextureHandle depth = {}) {
			FramebufferDesc fbDesc;
			fbDesc.width = kSize;
			fbDesc.height = kSize;
			fbDesc.layers = 1;
			fbDesc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = target;
			att.layer = UINT32_MAX;
			fbDesc.colorAttachments = { att };
			if (depth) {
				FramebufferAttachmentDesc depthAtt;
				depthAtt.texture = depth;
				depthAtt.layer = UINT32_MAX;
				fbDesc.depthStencilAttachment = depthAtt;
			}
			const auto fb = rhi.createFramebuffer(fbDesc);
			if (!fb) {
				return {};
			}

			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			for (const auto& step : steps) {
				if (!step.pipeline) {
					continue;
				}
				if (step.scissor) {
					rhi.cmdSetScissor(cmd, step.scissor->x, step.scissor->y, step.scissor->width, step.scissor->height);
				}
				rhi.cmdBindPipeline(cmd, step.pipeline);
				if (step.vertices) {
					// After the pipeline, never before: the OpenGL backend resolves the binding's stride
					// out of the currently bound pipeline's layout, so a vertex buffer bound ahead of one
					// is dropped without a word.
					rhi.cmdBindVertexBuffer(cmd, 0, step.vertices, 0);
				}
				else {
					rhi.cmdPushConstants(cmd, step.pipeline, ShaderStage::Vertex | ShaderStage::Fragment,
						&step.push, sizeof(step.push));
				}
				rhi.cmdDraw(cmd, step.vertexCount, 1, 0, 0);
			}
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();

			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			if (!rhi.readbackTexture(target, pixels, w, h)) {
				return {};
			}
			return pixels;
		}

		Rgba pixelAt(const std::vector<uint8_t>& pixels, int x, int y) {
			const size_t offset = (static_cast<size_t>(y) * kSize + static_cast<size_t>(x)) * 4u;
			if (offset + 3 >= pixels.size()) {
				return {};
			}
			return { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
		}

		// The same z on all three corners, so the interpolated depth is that constant everywhere the
		// triangle covers and no rasteriser's interpolation rule creeps into a depth comparison.
		TrianglePush fullScreen(const Math::Vec4& color, float z = 0.f) {
			TrianglePush push;
			push.a = { -1.f, -1.f, z, 1.f };
			push.b = { 3.f, -1.f, z, 1.f };
			push.c = { -1.f, 3.f, z, 1.f };
			push.color = color;
			return push;
		}

		// Covers the positive-Y half of clip space, which readback presents as the top rows. Half a
		// target rather than all of it, so one image holds both the blended pixels and untouched base
		// pixels that prove the first draw really landed.
		TrianglePush topHalf(const Math::Vec4& color) {
			TrianglePush push;
			push.a = { -1.f, 0.f, 0.f, 1.f };
			push.b = { 3.f, 0.f, 0.f, 1.f };
			push.c = { -1.f, 4.f, 0.f, 1.f };
			push.color = color;
			return push;
		}

		// Depth comes back as 16 bits spread across R and G, R being the high byte — the same encoding
		// RhiConventionTests reads. Separate from drawSteps' colour readback because a depth-write test
		// needs both attachments of the same pass, and drawSteps has already waited for the GPU by the
		// time it returns, so the stored depth is there to be read.
		int depthAt(RenderHardwareInterface& rhi, TextureHandle depth, int x, int y) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			if (!rhi.readbackTexture(depth, pixels, w, h)) {
				return -1;
			}
			const auto texel = pixelAt(pixels, x, y);
			return (static_cast<int>(texel.r) << 8) | static_cast<int>(texel.g);
		}

		constexpr float unorm(int byteValue) { return static_cast<float>(byteValue) / 255.f; }

		// Both draws use channel values that are multiples of 4/255 and a source alpha of exactly
		// 0.25, so every blended colour channel is an exact 8-bit value and no rounding rule is under
		// test here: 32*0.25 + 64*0.75 is 56, not 55.9 or 56.1. That is the point of avoiding alpha
		// 0.5, where 255*0.5 = 127.5 sits on a tie and implementations may land either side of it.
		const Math::Vec4 kBaseColor{ unorm(64), unorm(128), unorm(192), 1.f };
		const Math::Vec4 kOverColor{ unorm(32), unorm(96), unorm(160), 0.25f };
	}

	// What a BlendStateDesc actually computes. The engine's transparent passes and the ImGui overlay
	// both lean on SrcAlpha/OneMinusSrcAlpha meaning the classic over operator, and on a disabled
	// blend meaning a plain overwrite regardless of the fragment's alpha; if the two backends differ
	// by so much as a rounding step, every alpha-blended surface renders a different colour depending
	// on which backend the editor booted with.
	TEST_P(RhiBackendTest, BlendStateFactorsProduceTheSameBytesOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta, matching neither draw: if the base draw silently did nothing, the "over" draw
		// would blend against this instead and the numbers below would not survive.
		const auto pass = makeClearPass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		const auto basePipeline = makePipeline(rhi(), pass, PipelineOptions{});
		ASSERT_TRUE(basePipeline) << "pipeline creation failed";

		const auto runCase = [&](const BlendStateDesc& blend) {
			PipelineOptions options;
			options.blend = blend;
			const auto overPipeline = makePipeline(rhi(), pass, options);
			EXPECT_TRUE(overPipeline) << "blend pipeline creation failed";
			return drawSteps(rhi(), target, pass, {
				{ basePipeline, fullScreen(kBaseColor) },
				{ overPipeline, topHalf(kOverColor) },
				});
		};

		constexpr int kOverlapY = 1;         // inside both triangles
		constexpr int kBaseOnlyY = kSize - 2; // inside the base triangle only
		constexpr int kX = kSize / 2;

		// Disabled: the second draw replaces the first outright, and its alpha 0.25 changes nothing
		// about the colour channels — it is only written into the attachment's alpha.
		{
			BlendStateDesc blend;
			blend.enabled = false;
			const auto pixels = runCase(blend);
			ASSERT_FALSE(pixels.empty());
			EXPECT_EQ(pixelAt(pixels, kX, kBaseOnlyY), (Rgba{ 64, 128, 192, 255 }))
				<< "the base draw did not lay down its colour";
			const auto over = pixelAt(pixels, kX, kOverlapY);
			EXPECT_EQ(over.r, 32) << "disabled blending mixed the source with the destination";
			EXPECT_EQ(over.g, 96) << "disabled blending mixed the source with the destination";
			EXPECT_EQ(over.b, 160) << "disabled blending mixed the source with the destination";
			// 0.25 * 255 = 63.75. It is not a representable 8-bit value, but it is nowhere near a
			// tie, so both backends round it the same way and 64 can be pinned exactly.
			EXPECT_EQ(over.a, 64) << "the source alpha was not written through";
		}

		// SrcAlpha / OneMinusSrcAlpha: src*0.25 + dst*0.75, the over operator every transparent
		// surface in the engine assumes.
		{
			BlendStateDesc blend;
			blend.enabled = true;
			blend.srcColor = BlendFactor::SrcAlpha;
			blend.dstColor = BlendFactor::OneMinusSrcAlpha;
			const auto pixels = runCase(blend);
			ASSERT_FALSE(pixels.empty());
			EXPECT_EQ(pixelAt(pixels, kX, kBaseOnlyY), (Rgba{ 64, 128, 192, 255 }))
				<< "the base draw did not lay down its colour";
			const auto over = pixelAt(pixels, kX, kOverlapY);
			EXPECT_EQ(over.r, 56) << "32 * 0.25 + 64 * 0.75 is 56";
			EXPECT_EQ(over.g, 120) << "96 * 0.25 + 128 * 0.75 is 120";
			EXPECT_EQ(over.b, 184) << "160 * 0.25 + 192 * 0.75 is 184";
			// The alpha channel takes the same two factors as colour on both backends, so it computes
			// 0.25*0.25 + 1.0*0.75 = 0.8125, i.e. 207.1875 in 8 bits. Off a tie again, so both round
			// to 207 and the byte is pinned exactly rather than with a tolerance.
			EXPECT_EQ(over.a, 207) << "alpha was not blended with the same factors as colour";
		}

		// One / Zero: blending switched on but arithmetically an overwrite. Worth pinning separately
		// because it is the state a pass reaches for when it wants blending "off" without swapping
		// pipelines, and it must land on the same bytes as the disabled case.
		{
			BlendStateDesc blend;
			blend.enabled = true;
			blend.srcColor = BlendFactor::One;
			blend.dstColor = BlendFactor::Zero;
			const auto pixels = runCase(blend);
			ASSERT_FALSE(pixels.empty());
			EXPECT_EQ(pixelAt(pixels, kX, kBaseOnlyY), (Rgba{ 64, 128, 192, 255 }))
				<< "the base draw did not lay down its colour";
			const auto over = pixelAt(pixels, kX, kOverlapY);
			EXPECT_EQ(over.r, 32) << "One/Zero kept part of the destination";
			EXPECT_EQ(over.g, 96) << "One/Zero kept part of the destination";
			EXPECT_EQ(over.b, 160) << "One/Zero kept part of the destination";
			EXPECT_EQ(over.a, 64) << "One/Zero did not write the source alpha through";
		}
	}

	// What a depth compare actually decides. Every pass in the engine that draws twice into the same
	// depth buffer — the depth prepass and the g-buffer behind it, the glass pass reading LessEqual
	// against already-written opaque depth — relies on the op meaning the same thing on both
	// backends. It is also the one piece of pipeline state where a backend can be wrong in a way that
	// still looks plausible: swap Less for LessEqual and coplanar geometry flickers instead of
	// disappearing, which is a bug report months later rather than a red test now.
	//
	// The z values are clip space, which ClipSpaceZeroDepthStoresMidRangeOnBothBackends already pins
	// as the OpenGL [-1, 1] range on both backends (Vulkan matching through depth clip control), so
	// -0.5 / 0 / +0.5 mean nearer / exactly equal / farther than the base draw on both.
	TEST_P(RhiBackendTest, DepthCompareOpKeepsAndRejectsTheSameFragmentsOnBothBackends) {
		const auto color = makeTarget(rhi());
		ASSERT_TRUE(color);
		const auto depth = makeDepthTarget(rhi());
		ASSERT_TRUE(depth);
		// Blue clear, matching neither draw: a blue pixel means the base draw never landed, which is
		// a different failure from "the compare op rejected the second draw", and the assertions below
		// can tell them apart.
		const auto pass = makeColorDepthPass(rhi(), { 0.f, 0.f, 1.f, 1.f }, 1.f);
		ASSERT_TRUE(pass);

		// Always, so the base draw lands against the cleared far depth whatever the compare table
		// says, and writes its own z for the second draw to be compared against.
		PipelineOptions baseOptions;
		baseOptions.depthStencil = { true, true, CompareOp::Always };
		const auto basePipeline = makePipeline(rhi(), pass, baseOptions);
		ASSERT_TRUE(basePipeline) << "base pipeline creation failed";

		struct Probe {
			const char* label;
			float z;
		};
		// Against a depth buffer holding clip z = 0.
		static constexpr Probe kProbes[] = {
			{ "nearer", -0.5f },
			{ "equal",   0.0f },
			{ "farther", 0.5f },
		};
		constexpr size_t kProbeCount = sizeof(kProbes) / sizeof(kProbes[0]);

		struct OpCase {
			const char* name;
			CompareOp op;
			bool keeps[kProbeCount]; // nearer, equal, farther
		};
		// The whole table in one place: the "equal" column is what separates Less from LessEqual and
		// Greater from GreaterEqual, and the reason a nearer-only test would pass on a backend that
		// confused the two.
		static constexpr OpCase kCases[] = {
			{ "Less",         CompareOp::Less,         { true,  false, false } },
			{ "LessEqual",    CompareOp::LessEqual,    { true,  true,  false } },
			{ "Greater",      CompareOp::Greater,      { false, false, true  } },
			{ "GreaterEqual", CompareOp::GreaterEqual, { false, true,  true  } },
			{ "Equal",        CompareOp::Equal,        { false, true,  false } },
			{ "NotEqual",     CompareOp::NotEqual,     { true,  false, true  } },
			{ "Never",        CompareOp::Never,        { false, false, false } },
			{ "Always",       CompareOp::Always,       { true,  true,  true  } },
			// CompareOp::None is the sampler enum's "no comparison at all", and neither backend has a
			// separate meaning for it in a depth state: both fall through to Always. Pinned as Always
			// so that if either backend ever reinterprets it as "no depth test" — which would be the
			// other defensible reading — this row says so rather than a pass silently changing.
			{ "None",         CompareOp::None,         { true,  true,  true  } },
		};

		const Math::Vec4 kBaseDraw{ 0.f, 1.f, 0.f, 1.f };
		const Math::Vec4 kTestDraw{ 1.f, 0.f, 0.f, 1.f };
		const Rgba kKept{ 255, 0, 0, 255 };     // the second draw survived the compare
		const Rgba kRejected{ 0, 255, 0, 255 }; // only the base draw is visible
		constexpr int kX = kSize / 2;
		constexpr int kY = kSize / 2;

		for (const auto& testCase : kCases) {
			PipelineOptions options;
			// depthWrite stays on for both draws: this test reads only colour, and leaving it at the
			// default keeps the result independent of what depthWrite means, which is its own checkbox.
			options.depthStencil = { true, true, testCase.op };
			const auto pipeline = makePipeline(rhi(), pass, options);
			ASSERT_TRUE(pipeline) << testCase.name << ": pipeline creation failed";

			for (size_t i = 0; i < kProbeCount; ++i) {
				const auto pixels = drawSteps(rhi(), color, pass, {
					{ basePipeline, fullScreen(kBaseDraw, 0.f) },
					{ pipeline, fullScreen(kTestDraw, kProbes[i].z) },
					}, depth);
				ASSERT_FALSE(pixels.empty()) << testCase.name << " / " << kProbes[i].label << ": readback failed";
				const auto pixel = pixelAt(pixels, kX, kY);
				EXPECT_EQ(pixel, testCase.keeps[i] ? kKept : kRejected)
					<< testCase.name << " with a " << kProbes[i].label << " z (" << kProbes[i].z
					<< "): the fragment was "
					<< (testCase.keeps[i] ? "rejected but should have been kept"
										  : "kept but should have been rejected");
			}
		}
	}

	// What depthWrite actually suppresses. The engine's transparent and overlay passes draw against
	// opaque depth with writes off, so that the fragments test against the solid geometry in front of
	// them without occluding each other; if a backend wrote depth anyway, the first transparent
	// surface would hide the ones behind it and the pass would still look almost right — which is why
	// this asserts on the number in the depth attachment and not only on the colour. A colour-only
	// check passes whether or not the write was suppressed, because in both cases the fragment is
	// kept: depthWrite has no say in the compare.
	//
	// Depth values below are window depth, (clip z + 1) / 2 scaled to 16 bits, the mapping
	// ClipSpaceZeroDepthStoresMidRangeOnBothBackends pins on both backends.
	TEST_P(RhiBackendTest, DepthWriteOffLeavesTheStoredDepthAloneOnBothBackends) {
		const auto color = makeTarget(rhi());
		ASSERT_TRUE(color);
		const auto depth = makeDepthTarget(rhi());
		ASSERT_TRUE(depth);
		// Blue clear again, matching neither draw, so "the base draw never landed" reads differently
		// from "the second draw was rejected".
		const auto pass = makeColorDepthPass(rhi(), { 0.f, 0.f, 1.f, 1.f }, 1.f);
		ASSERT_TRUE(pass);

		constexpr float kBaseZ = -0.5f;  // stored as 0.25 of the range
		constexpr float kOverZ = 0.0f;   // 0.5: farther than the base, so a Greater compare keeps it
		constexpr float kFailZ = 0.5f;   // 0.75: a Less compare against the base would reject it
		constexpr int kBaseStored = 16384;
		constexpr int kOverStored = 32768;
		constexpr int kFailStored = 49152;
		constexpr int kClearStored = 65535;
		// 16 bits of depth arrive through an 8-bit-per-channel readback, so the low byte carries
		// whatever the format rounded to; ±512 is the tolerance the convention test already uses. The
		// three z values sit 16384 apart, so a tolerance this small still cannot let a wrong depth
		// pass for a right one.
		constexpr int kTolerance = 512;

		// Always, so the base draw lands against the cleared far depth and leaves kBaseStored behind
		// for the second draw to be compared against and, in the control case, to overwrite.
		PipelineOptions baseOptions;
		baseOptions.depthStencil = { true, true, CompareOp::Always };
		const auto basePipeline = makePipeline(rhi(), pass, baseOptions);
		ASSERT_TRUE(basePipeline) << "base pipeline creation failed";

		const Math::Vec4 kBaseDraw{ 0.f, 1.f, 0.f, 1.f };
		const Math::Vec4 kOverDraw{ 1.f, 0.f, 0.f, 1.f };
		const Rgba kKept{ 255, 0, 0, 255 };     // the second draw is on screen
		const Rgba kRejected{ 0, 255, 0, 255 }; // only the base draw is
		constexpr int kX = kSize / 2;
		constexpr int kY = kSize / 2;

		struct Result {
			Rgba color;
			int depth = -1;
		};
		const auto runCase = [&](const DepthStencilStateDesc& state, float z) {
			PipelineOptions options;
			options.depthStencil = state;
			const auto pipeline = makePipeline(rhi(), pass, options);
			EXPECT_TRUE(pipeline) << "pipeline creation failed";
			const auto pixels = drawSteps(rhi(), color, pass, {
				{ basePipeline, fullScreen(kBaseDraw, kBaseZ) },
				{ pipeline, fullScreen(kOverDraw, z) },
				}, depth);
			EXPECT_FALSE(pixels.empty()) << "colour readback failed";
			Result result;
			result.color = pixelAt(pixels, kX, kY);
			result.depth = depthAt(rhi(), depth, kX, kY);
			return result;
		};

		// The case under test: the second draw passes the compare, so its colour must be there, but
		// with writes off the attachment must still hold the first draw's z.
		{
			const auto result = runCase({ true, false, CompareOp::Greater }, kOverZ);
			EXPECT_EQ(result.color, kKept) << "depthWrite = false must not affect whether the fragment passes";
			EXPECT_NEAR(result.depth, kBaseStored, kTolerance)
				<< "depthWrite = false still wrote the second draw's depth; stored " << result.depth
				<< ", expected the first draw's " << kBaseStored;
		}

		// The control, identical but for the one flag, so a failure in the case above cannot be blamed
		// on the base draw, the compare op or the readback.
		{
			const auto result = runCase({ true, true, CompareOp::Greater }, kOverZ);
			EXPECT_EQ(result.color, kKept) << "the second draw did not pass a Greater compare";
			EXPECT_NEAR(result.depth, kOverStored, kTolerance)
				<< "depthWrite = true did not store the second draw's depth; stored " << result.depth;
		}

		// A fragment the compare would reject, drawn with the test switched off: it must be kept, and
		// the depth it leaves behind is the interesting half. Both APIs say writes are disabled while
		// the depth test is off — GL updates the buffer only through the test, Vulkan ignores
		// depthWriteEnable when depthTestEnable is false — so the base draw's z must survive even
		// though depthWrite is on. Pinned because a backend that reached for a fixed always-pass
		// compare instead of disabling the test would keep the colour identical and quietly overwrite
		// depth here, which nothing else in the suite would notice.
		{
			const auto result = runCase({ false, true, CompareOp::Less }, kFailZ);
			EXPECT_EQ(result.color, kKept) << "depthTest = false must let the fragment through regardless of the compare";
			EXPECT_NEAR(result.depth, kBaseStored, kTolerance)
				<< "depthTest = false wrote depth anyway; stored " << result.depth << ", expected the base draw's "
				<< kBaseStored << " (the rejected fragment's own depth would be " << kFailStored << ")";
		}

		// The same state with nothing drawn before it, to separate "the write was suppressed" from
		// "the write happened to land on the value that was already there": with no base draw the
		// attachment must still hold the clear.
		{
			PipelineOptions options;
			options.depthStencil = { false, true, CompareOp::Less };
			const auto pipeline = makePipeline(rhi(), pass, options);
			ASSERT_TRUE(pipeline) << "pipeline creation failed";
			const auto pixels = drawSteps(rhi(), color, pass, { { pipeline, fullScreen(kOverDraw, kFailZ) } }, depth);
			ASSERT_FALSE(pixels.empty()) << "colour readback failed";
			EXPECT_EQ(pixelAt(pixels, kX, kY), kKept) << "the only draw in the pass did not land";
			EXPECT_NEAR(depthAt(rhi(), depth, kX, kY), kClearStored, kTolerance)
				<< "depthTest = false wrote depth over the cleared attachment";
		}
	}

	// Which corner cmdSetScissor's rectangle is measured from. This is the one piece of pipeline
	// state where the two APIs start from opposite ends: OpenGL's glScissor origin is the bottom-left
	// of the framebuffer, Vulkan's VkRect2D offset is the top-left of the image. An RHI that passed
	// both through unchanged would clip ImGui's window rects — the editor's only caller — to a
	// mirrored band on one backend, which reads as "the panel clips wrong" rather than as an origin
	// bug, and is invisible to any test whose rectangle happens to be vertically centred.
	//
	// So the rectangle here is deliberately off-centre on both axes: the top quarter of the target,
	// pushed to the right. A y-flip, an x-mirror and a transpose each move it somewhere the
	// assertions below can see. Readback normalises rows top-down on both backends (the same
	// orientation topHalf relies on), so there is one set of numbers, not one per backend.
	TEST_P(RhiBackendTest, ScissorClipsToTheSameOffCentreRectangleOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue clear against a red draw: outside the rectangle the clear must survive untouched, so
		// "the scissor rejected the fragment" and "the draw never happened" are the same colour only
		// if the draw landed nowhere at all — which the inside-pixel assertion rules out.
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		const auto pipeline = makePipeline(rhi(), pass, PipelineOptions{});
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		// x 5..6, y 1..2 of an 8x8 target. Off-centre both ways and not square-symmetric about either
		// axis: the y-mirror of this rectangle starts at y 5, the x-mirror at x 1, and the transpose
		// at (1, 5), so no accidental symmetry lets a wrong origin pass.
		constexpr int32_t kRectX = 5;
		constexpr int32_t kRectY = 1;
		constexpr uint32_t kRectW = 2;
		constexpr uint32_t kRectH = 2;

		const Math::Vec4 kDrawColor{ 1.f, 0.f, 0.f, 1.f };
		const Rgba kDrawn{ 255, 0, 0, 255 };
		const Rgba kClear{ 0, 0, 255, 255 };

		{
			const auto pixels = drawSteps(rhi(), target, pass, {
				{ pipeline, fullScreen(kDrawColor), ScissorRect{ kRectX, kRectY, kRectW, kRectH } },
				});
			ASSERT_FALSE(pixels.empty()) << "readback failed";

			// Both corners of the rectangle, so its size is pinned as well as its origin: a rectangle
			// one pixel off in either direction misses one of these.
			EXPECT_EQ(pixelAt(pixels, kRectX, kRectY), kDrawn)
				<< "the top-left pixel of the scissor rectangle was clipped away; cmdSetScissor's origin "
				   "is not the top-left corner of the readback image";
			EXPECT_EQ(pixelAt(pixels, kRectX + static_cast<int>(kRectW) - 1, kRectY + static_cast<int>(kRectH) - 1), kDrawn)
				<< "the bottom-right pixel of the scissor rectangle was clipped away";

			// All four sides. Above and below catch a y-flip, left and right catch an x-mirror; the two
			// together catch a transpose, which a y-only check would let through.
			EXPECT_EQ(pixelAt(pixels, kRectX, kRectY - 1), kClear)
				<< "a pixel above the rectangle was drawn: the rectangle sits higher than asked, i.e. its "
				   "y origin is measured from the wrong edge";
			EXPECT_EQ(pixelAt(pixels, kRectX, kRectY + static_cast<int>(kRectH)), kClear)
				<< "a pixel below the rectangle was drawn";
			EXPECT_EQ(pixelAt(pixels, kRectX - 1, kRectY), kClear)
				<< "a pixel left of the rectangle was drawn: the x origin is mirrored or transposed";
			EXPECT_EQ(pixelAt(pixels, kRectX + static_cast<int>(kRectW), kRectY), kClear)
				<< "a pixel right of the rectangle was drawn";

			// Nothing anywhere else either. Named pixels prove the four edges; this proves the
			// rectangle did not also leak into a corner none of them looks at — a backend that clipped
			// to the union of the asked-for rect and its mirror would pass every assertion above.
			int drawnOutside = 0;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					const bool inside = x >= kRectX && x < kRectX + static_cast<int>(kRectW)
						&& y >= kRectY && y < kRectY + static_cast<int>(kRectH);
					if (!inside && !(pixelAt(pixels, x, y) == kClear)) {
						++drawnOutside;
					}
				}
			}
			EXPECT_EQ(drawnOutside, 0) << "the draw reached " << drawnOutside
				<< " pixels outside the scissor rectangle";
		}

		// The same draw with no cmdSetScissor at all. Pins the default rather than assuming it: the
		// OpenGL backend has to explicitly disable GL_SCISSOR_TEST when a pass begins and Vulkan has to
		// set a full-target VkRect2D, and if either forgot, every pass that never touches the scissor
		// would silently inherit whatever rectangle the previous one left behind.
		{
			const auto pixels = drawSteps(rhi(), target, pass, { { pipeline, fullScreen(kDrawColor) } });
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			int notDrawn = 0;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					if (!(pixelAt(pixels, x, y) == kDrawn)) {
						++notDrawn;
					}
				}
			}
			EXPECT_EQ(notDrawn, 0) << notDrawn
				<< " pixels stayed at the clear colour with no scissor set: the default scissor is not the "
				   "whole target";
		}
	}

	// A degenerate rectangle means "reject everything", not "reject almost everything". ImGui asks
	// for one whenever a window is fully clipped — a panel dragged off screen, a collapsed child —
	// so this input arrives every frame in the editor, not only in a corner case. OpenGL has always
	// answered it exactly (`glScissor(0, 0, 0, 0)` with the test enabled), while the Vulkan path
	// substituted a 1x1 rectangle at the origin, because a VkRect2D of zero extent looked like the
	// degenerate case worth avoiding. The result was a single stray pixel painted on Vulkan and none
	// on OpenGL: too small to notice in a viewport, and invisible to any test that samples the middle
	// of the target. Hence the whole-image scan below rather than a handful of named pixels — the
	// pixel that leaked was the one at the origin, which is exactly the pixel a spot check skips.
	TEST_P(RhiBackendTest, EmptyScissorRectangleRejectsEveryPixelOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue clear against a red draw, as above: any pixel that is not the clear colour was painted
		// by a draw the scissor was supposed to reject entirely.
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		const auto pipeline = makePipeline(rhi(), pass, PipelineOptions{});
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const Math::Vec4 kDrawColor{ 1.f, 0.f, 0.f, 1.f };
		const Rgba kDrawn{ 255, 0, 0, 255 };
		const Rgba kClear{ 0, 0, 255, 255 };

		// Counts every pixel the draw reached, and remembers the first one, so a failure names a
		// coordinate instead of only a count: "1 pixel at (0, 7)" is the old Vulkan bug by sight.
		const auto drawnPixels = [&](const std::vector<uint8_t>& pixels, int& firstX, int& firstY) {
			int drawn = 0;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					if (!(pixelAt(pixels, x, y) == kClear)) {
						if (drawn == 0) {
							firstX = x;
							firstY = y;
						}
						++drawn;
					}
				}
			}
			return drawn;
		};

		// Off-centre and away from every edge, so a backend that clamped a degenerate rectangle to
		// the target would land somewhere the scan can see rather than harmlessly outside it. The
		// three cases separate the two degenerate axes: a backend that special-cased only width == 0
		// still fails the zero-height row.
		struct Case {
			const char* label;
			ScissorRect rect;
		};
		const Case kCases[] = {
			{ "zero width",           ScissorRect{ 3, 2, 0, 4 } },
			{ "zero height",          ScissorRect{ 3, 2, 4, 0 } },
			{ "zero width and height", ScissorRect{ 3, 2, 0, 0 } },
			// The origin itself: the pixel the old 1x1 Vulkan fallback let through sat at a corner, so
			// a rectangle asked for there makes "the fallback fired" and "the rectangle was honoured"
			// indistinguishable by position — nothing may be painted either way.
			{ "zero extent at the origin", ScissorRect{ 0, 0, 0, 0 } },
		};

		for (const auto& testCase : kCases) {
			const auto pixels = drawSteps(rhi(), target, pass, {
				{ pipeline, fullScreen(kDrawColor), testCase.rect },
				});
			ASSERT_FALSE(pixels.empty()) << testCase.label << ": readback failed";

			int firstX = -1;
			int firstY = -1;
			const int drawn = drawnPixels(pixels, firstX, firstY);
			EXPECT_EQ(drawn, 0) << "a " << testCase.label << " scissor let " << drawn
				<< " pixel(s) through, the first at (" << firstX << ", " << firstY
				<< "): an empty rectangle must reject every fragment, not fall back to a rectangle of "
				   "its own";
		}

		// The control. "Nothing was painted" is the kind of assertion that passes just as happily
		// when the draw never ran at all — a pipeline that failed to bind, a push constant the shader
		// ignored — so the same target, pass, pipeline and triangle are run once more with a
		// non-degenerate rectangle covering the same area the zero-width case asked for. If this
		// paints its four pixels, the cases above were rejected by the scissor and by nothing else.
		{
			const auto pixels = drawSteps(rhi(), target, pass, {
				{ pipeline, fullScreen(kDrawColor), ScissorRect{ 3, 2, 4, 4 } },
				});
			ASSERT_FALSE(pixels.empty()) << "control: readback failed";
			int painted = 0;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					if (pixelAt(pixels, x, y) == kDrawn) {
						++painted;
					}
				}
			}
			EXPECT_EQ(painted, 16) << "the same draw with a non-empty 4x4 rectangle painted " << painted
				<< " pixels instead of 16: the empty-scissor cases above prove nothing, because this "
				   "setup does not draw even when the scissor allows it";
		}
	}

	// GraphicsPipelineDesc::fillMode, which is the only way to ask for wireframe. There used to be a
	// dynamic cmdSetPolygonFillMode as well; it worked on OpenGL and was an empty override on Vulkan,
	// so the editor's wireframe toggle silently rendered solid geometry there. The interface dropped it
	// and GeometryPass now keeps a second pipeline keyed by fill mode, which is the only shape Vulkan
	// can express: fill is baked into the PSO.
	//
	// The triangle is a right triangle whose three corners sit exactly on pixel centres of the 8x8
	// target: (0, 7), (7, 7) and (0, 0) in readback pixels. That puts its bottom edge along the centres
	// of row 7 and its left edge along the centres of column 0, so line rasterisation covers exactly
	// those pixels on both backends — no diamond-exit rule is being leaned on, and the probes sit
	// mid-edge, away from the endpoints where that rule does decide. Nothing is asserted about pixels
	// sitting on an edge under Fill: the two backends rasterise into oppositely oriented framebuffer
	// space (Vulkan flips the viewport), so the top-left fill rule can legitimately resolve an
	// exactly-on-edge pixel differently, and the interior set below stays clear of the edges for that
	// reason.
	TEST_P(RhiBackendTest, BakedPolygonFillModeLineDrawsOnlyTheTriangleEdgesOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue clear against a red draw: an unpainted interior is the clear colour, so "Line skipped the
		// interior" and "the draw never happened" look identical — which is why every case also asserts
		// on an edge pixel and on a painted count.
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		PipelineOptions lineOptions;
		lineOptions.fill = PolygonFillMode::Line;
		const auto fillPipeline = makePipeline(rhi(), pass, PipelineOptions{});
		ASSERT_TRUE(fillPipeline) << "Fill pipeline creation failed";
		const auto linePipeline = makePipeline(rhi(), pass, lineOptions);
		// Vulkan needs the fillModeNonSolid device feature for VK_POLYGON_MODE_LINE. VulkanRHI enables
		// it when the physical device reports it; if it does not, a Line pipeline cannot be built at all
		// and there is nothing here to hold the two backends to.
		if (!linePipeline) {
			GTEST_SKIP() << "a PolygonFillMode::Line pipeline could not be created: on Vulkan this means "
							"the device does not support the fillModeNonSolid feature";
		}

		// Pixel centres of an 8x8 target, in clip space.
		constexpr auto centreX = [](int col) { return (static_cast<float>(col) + 0.5f) / 4.f - 1.f; };
		constexpr auto centreY = [](int row) { return 1.f - (static_cast<float>(row) + 0.5f) / 4.f; };

		const Math::Vec4 kDrawColor{ 1.f, 0.f, 0.f, 1.f };
		const Rgba kDrawn{ 255, 0, 0, 255 };
		const Rgba kClear{ 0, 0, 255, 255 };

		TrianglePush triangle;
		triangle.a = { centreX(0), centreY(7), 0.f, 1.f };
		triangle.b = { centreX(7), centreY(7), 0.f, 1.f };
		triangle.c = { centreX(0), centreY(0), 0.f, 1.f };
		triangle.color = kDrawColor;

		// Ten pixels at least one clear pixel inside every edge, so no one-pixel-wide line along any
		// edge can reach them. A single interior probe would let through a backend that thickened its
		// lines everywhere except at that one coordinate; this is the whole interior instead.
		const auto strictlyInside = [](int x, int y) { return x >= 1 && y <= 6 && y - x >= 2; };
		constexpr int kInsideCount = 10;
		// Mid-edge, far from both endpoints, where a line is covered under every rasterisation rule.
		constexpr int kBottomEdgeX = 3;
		constexpr int kBottomEdgeY = 7;
		constexpr int kLeftEdgeX = 0;
		constexpr int kLeftEdgeY = 3;

		// Returns how many of the ten interior pixels the draw painted, and names the first pixel that
		// disagreed with what the case expected, so a failure reads as a coordinate and not a count.
		const auto paintedInside = [&](const std::vector<uint8_t>& pixels, bool expectFilled, int& oddX, int& oddY) {
			int painted = 0;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					if (!strictlyInside(x, y)) {
						continue;
					}
					const bool isPainted = pixelAt(pixels, x, y) == kDrawn;
					if (isPainted) {
						++painted;
					}
					if (isPainted != expectFilled && oddX < 0) {
						oddX = x;
						oddY = y;
					}
				}
			}
			return painted;
		};

		struct Case {
			const char* label;
			PipelineHandle pipeline;
			bool expectFilled;
		};
		const Case kCases[] = {
			{ "baked Fill", fillPipeline, true  },
			{ "baked Line", linePipeline, false },
		};

		for (const auto& testCase : kCases) {
			const DrawStep step{ testCase.pipeline, triangle };
			const auto pixels = drawSteps(rhi(), target, pass, { step });
			ASSERT_FALSE(pixels.empty()) << testCase.label << ": readback failed";

			int oddX = -1;
			int oddY = -1;
			const int inside = paintedInside(pixels, testCase.expectFilled, oddX, oddY);
			// Both directions in one place. "The interior is clear" is the kind of assertion that passes
			// just as happily when the draw never ran at all, so the two edge probes carry the other half
			// of the claim: the triangle was rasterised, it was only hollow.
			EXPECT_EQ(inside, testCase.expectFilled ? kInsideCount : 0)
				<< testCase.label << ": " << inside << " of the " << kInsideCount
				<< " interior pixels were painted, first disagreement at (" << oddX << ", " << oddY
				<< "); the triangle rasterised " << (testCase.expectFilled ? "hollow" : "solid");
			EXPECT_EQ(pixelAt(pixels, kBottomEdgeX, kBottomEdgeY), kDrawn)
				<< testCase.label << ": the bottom edge is unpainted, so the triangle was not rasterised "
				   "at all and the interior check above proves nothing";
			EXPECT_EQ(pixelAt(pixels, kLeftEdgeX, kLeftEdgeY), kDrawn)
				<< testCase.label << ": the left edge is unpainted";
		}
	}

	// GraphicsPipelineDesc::topology, which decides how a run of vertices is grouped into primitives.
	// Every other test in the suite draws Triangles, so the value here is the other three: a backend
	// that quietly grouped the vertices as triangles anyway would still paint something plausible, and
	// nothing else would catch it. Each case therefore asserts on a pixel only its own topology can
	// paint, and on a pixel it has to leave at the clear colour — the pixel a wrong grouping of the
	// very same vertices would have painted instead.
	//
	// Two enumerators are left out. Patches needs tessellation control and evaluation stages, which no
	// test shader has and which the interface only reaches through patchControlPoints; that is a test
	// with its own shaders, not a row in this table. LineStrip is left out because its interesting
	// half is the segment it must *not* draw, which wants a shape of its own — see
	// LineStripJoinsConsecutiveVerticesWithoutClosingBackToTheFirstOnBothBackends.
	//
	// Sizes are deliberately not pinned. A one-pixel point on one backend and a wider splat on the
	// other is a legitimate difference, as is line width, so every assertion is either on the pixel a
	// shape's centre must cover or on a pixel at least three pixels clear of every shape drawn.
	TEST_P(RhiBackendTest, PrimitiveTopologyGroupsVerticesIntoStripsLinesAndPointsOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue clear against a white draw: every "stays clear" pixel below is a pixel some other
		// grouping of the same vertices would have painted, so a wrong topology shows as a colour.
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		const Rgba kDrawn{ 255, 255, 255, 255 };
		const Rgba kClear{ 0, 0, 255, 255 };
		// White in the vertex record's colour slot, so the pixel says only whether it was covered.
		constexpr float kW = 1.f;

		// TriangleStrip: four vertices are two triangles sharing an edge, i.e. one quad. The same four
		// under Triangles are one triangle and a dropped fourth vertex, so the half of the quad on the
		// far side of the shared edge is the discriminating region.
		{
			// A 4x6 quad in the top-left rather than a square one: the shared edge runs from the
			// bottom-left corner to the top-right, and on a square that diagonal passes through four
			// pixel centres exactly. A pixel centre sitting on the edge between the two triangles is
			// decided by the fill rule, and the two backends rasterise into oppositely oriented
			// framebuffer space, so they may legitimately resolve it differently. At 4x6 the edge is
			// y = 6 - 1.5x in pixels, which lands on quarter offsets and misses every centre.
			const Vertex quad[4] = {
				{ -1.f,  1.0f, kW, kW, kW, kW }, // top-left
				{ -1.f, -0.5f, kW, kW, kW, kW }, // bottom-left
				{  0.f,  1.0f, kW, kW, kW, kW }, // top-right
				{  0.f, -0.5f, kW, kW, kW, kW }, // bottom-right
			};
			const auto vbo = makeVertexBuffer(rhi(), quad, 4);
			ASSERT_TRUE(vbo);
			const auto pipeline = makeVertexPipeline(rhi(), pass, PrimitiveTopology::TriangleStrip);
			ASSERT_TRUE(pipeline) << "a TriangleStrip pipeline could not be created";

			DrawStep step;
			step.pipeline = pipeline;
			step.vertices = vbo;
			step.vertexCount = 4;
			const auto pixels = drawSteps(rhi(), target, pass, { step });
			ASSERT_FALSE(pixels.empty()) << "TriangleStrip: readback failed";

			// Inside the strip's first triangle, which Triangles would draw too: this says the draw ran
			// at all, so the assertion below is about the grouping and not about a dead pipeline.
			EXPECT_EQ(pixelAt(pixels, 0, 0), kDrawn) << "the TriangleStrip draw painted nothing at all";
			// The whole point. Only the strip's second triangle reaches this corner; under Triangles the
			// fourth vertex is dropped, there is no second triangle, and this pixel stays blue.
			EXPECT_EQ(pixelAt(pixels, 3, 5), kDrawn)
				<< "the far corner of the quad is unpainted: the four vertices were grouped as one "
				   "triangle, not as a strip of two";

			// And the quad and nothing but the quad. Its edges fall on clip x = 0 and y = -0.5, which
			// are pixel boundaries, so the covered set is exact rather than a rounding question; a
			// backend that stretched the strip into a fan or repeated a vertex lands outside it.
			int wrong = 0;
			int firstWrongX = -1;
			int firstWrongY = -1;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					const bool inside = x <= 3 && y <= 5;
					if ((pixelAt(pixels, x, y) == kDrawn) != inside) {
						if (wrong == 0) {
							firstWrongX = x;
							firstWrongY = y;
						}
						++wrong;
					}
				}
			}
			EXPECT_EQ(wrong, 0) << "the strip covered " << wrong
				<< " pixel(s) other than the 4x6 quad its four vertices describe, the first at ("
				<< firstWrongX << ", " << firstWrongY << ")";
		}

		// Lines: vertex pairs, each an independent segment, so four vertices are two segments with a
		// gap between them. The gap is the discriminating region — LineStrip would join
		// the second vertex to the third and fill it, and any triangle topology would find all four
		// vertices collinear and paint nothing at all.
		{
			// Both segments run along the centres of row 1, so every pixel they cover is covered under
			// any rasterisation rule and no diamond-exit decision is being leaned on. Pixels 0..2 and
			// 5..7, leaving 3 and 4 empty.
			const Vertex segments[4] = {
				{ pixelCentreX(0), pixelCentreY(1), kW, kW, kW, kW },
				{ pixelCentreX(2), pixelCentreY(1), kW, kW, kW, kW },
				{ pixelCentreX(5), pixelCentreY(1), kW, kW, kW, kW },
				{ pixelCentreX(7), pixelCentreY(1), kW, kW, kW, kW },
			};
			const auto vbo = makeVertexBuffer(rhi(), segments, 4);
			ASSERT_TRUE(vbo);
			const auto pipeline = makeVertexPipeline(rhi(), pass, PrimitiveTopology::Lines);
			ASSERT_TRUE(pipeline) << "a Lines pipeline could not be created";

			DrawStep step;
			step.pipeline = pipeline;
			step.vertices = vbo;
			step.vertexCount = 4;
			const auto pixels = drawSteps(rhi(), target, pass, { step });
			ASSERT_FALSE(pixels.empty()) << "Lines: readback failed";

			// The middle of each segment, away from both endpoints: the one place a line is covered
			// whatever the backend does with its ends. Nothing is asserted about the endpoint pixels,
			// because the last one is excluded under the diamond-exit rule, nor about the rows above
			// and below, because line width may legitimately differ between the two backends.
			EXPECT_EQ(pixelAt(pixels, 1, 1), kDrawn)
				<< "the first segment is unpainted: either Lines drew nothing, or the four collinear "
				   "vertices were grouped as a triangle";
			EXPECT_EQ(pixelAt(pixels, 6, 1), kDrawn)
				<< "the second segment is unpainted: the second vertex pair did not become its own line";
			// The gap. Width thickens a line across its length, never along it, so these two pixels stay
			// blue on any line width — but not if the vertices were joined into a strip.
			EXPECT_EQ(pixelAt(pixels, 3, 1), kClear)
				<< "the gap between the two segments was painted: vertex 1 was joined to vertex 2, so the "
				   "vertices were grouped as a strip rather than as independent pairs";
			EXPECT_EQ(pixelAt(pixels, 4, 1), kClear)
				<< "the gap between the two segments was painted";
		}

		// Points: one pixel per vertex, with clear space between them. Four vertices at the four
		// corners, which under Triangles would be one large corner-to-corner triangle and under
		// TriangleStrip the whole target — so the two interior probes below say which of the three
		// actually happened.
		{
			const Vertex points[4] = {
				{ pixelCentreX(0), pixelCentreY(0), kW, kW, kW, kW },
				{ pixelCentreX(7), pixelCentreY(0), kW, kW, kW, kW },
				{ pixelCentreX(0), pixelCentreY(7), kW, kW, kW, kW },
				{ pixelCentreX(7), pixelCentreY(7), kW, kW, kW, kW },
			};
			const auto vbo = makeVertexBuffer(rhi(), points, 4);
			ASSERT_TRUE(vbo);
			const auto pipeline = makeVertexPipeline(rhi(), pass, PrimitiveTopology::Points);
			ASSERT_TRUE(pipeline) << "a Points pipeline could not be created";

			DrawStep step;
			step.pipeline = pipeline;
			step.vertices = vbo;
			step.vertexCount = 4;
			const auto pixels = drawSteps(rhi(), target, pass, { step });
			ASSERT_FALSE(pixels.empty()) << "Points: readback failed";

			// Each vertex sits on a pixel centre, so the pixel containing it is covered whatever size
			// the backend gives a point. Nothing is asserted about the pixels around them: a one-pixel
			// point and a wider splat are both defensible, and the shader asks for size 1 rather than
			// leaving it undefined precisely so the two backends are asked for the same thing.
			EXPECT_EQ(pixelAt(pixels, 0, 0), kDrawn) << "the first point did not paint the pixel it sits in";
			EXPECT_EQ(pixelAt(pixels, 7, 0), kDrawn) << "the second point did not paint the pixel it sits in";
			EXPECT_EQ(pixelAt(pixels, 0, 7), kDrawn) << "the third point did not paint the pixel it sits in";
			EXPECT_EQ(pixelAt(pixels, 7, 7), kDrawn) << "the fourth point did not paint the pixel it sits in";

			// Three pixels clear of every vertex, so no plausible point size reaches either of them, and
			// each is inside a different wrong answer: (3, 3) is inside the triangle the first three
			// vertices would make under Triangles, and (4, 4) is outside that triangle but inside the
			// quad they would make under TriangleStrip.
			EXPECT_EQ(pixelAt(pixels, 3, 3), kClear)
				<< "the middle of the target was painted: the four vertices were grouped into a triangle "
				   "rather than drawn as four separate points";
			EXPECT_EQ(pixelAt(pixels, 4, 4), kClear)
				<< "the middle of the target was painted: the four vertices were grouped into a strip "
				   "rather than drawn as four separate points";
		}
	}

	// destroyGraphicsPipeline, which every caller in the engine reaches at a moment when something
	// else is still live: the ImGui backend rebuilds its pipeline when the font atlas changes, the
	// text renderer drops one in its destructor, and the editor swaps pipelines whenever a render
	// setting moves. Neither backend keeps a deferred deletion queue — OpenGL deletes the program and
	// the VAO inline, Vulkan calls vkDestroyPipeline inline — so what a destroy leaves behind is the
	// free-list slot and, on OpenGL, the fact that it also unbinds whatever was current. Both of those
	// are invisible until the *next* pipeline is used, which is why every destroy below is followed by
	// a draw whose pixels say the survivor still works.
	//
	// Which pipeline is bound is read out of a colour rather than out of a handle id: two pipelines
	// differing only in blend state paint the overlap pixel differently, so a bind that landed on a
	// stale slot, or a recycled slot that kept the old pipeline's state, shows up as the wrong number.
	// The two colours are the ones BlendStateFactorsProduceTheSameBytesOnBothBackends already pins, so
	// nothing new about blending is being claimed here.
	TEST_P(RhiBackendTest, GraphicsPipelineDestroyAndRecreateKeepsTheLiveHandleWorkingOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta, matching neither draw: if the base draw silently stopped working after a destroy,
		// the overlap pixel would be blended against this instead and the numbers below would not
		// survive — "the survivor broke" and "the new pipeline is wrong" stay distinguishable.
		const auto pass = makeClearPass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		// Alive for the whole test, and never the pipeline being destroyed: it draws first in every
		// frame below, so a failure on the bottom half names the survivor and a failure on the overlap
		// names the pipeline that was just created.
		const auto basePipeline = makePipeline(rhi(), pass, PipelineOptions{});
		ASSERT_TRUE(basePipeline) << "base pipeline creation failed";

		PipelineOptions opaqueOptions; // blending disabled: the over draw replaces the base outright
		PipelineOptions blendedOptions;
		blendedOptions.blend.enabled = true;
		blendedOptions.blend.srcColor = BlendFactor::SrcAlpha;
		blendedOptions.blend.dstColor = BlendFactor::OneMinusSrcAlpha;

		const Rgba kBase{ 64, 128, 192, 255 };    // the base draw, untouched
		const Rgba kOpaque{ 32, 96, 160, 64 };    // the over draw with blending off
		const Rgba kBlended{ 56, 120, 184, 207 }; // the over draw mixed 0.25 / 0.75 with the base
		constexpr int kOverlapY = 1;              // inside both triangles
		constexpr int kBaseOnlyY = kSize - 2;     // inside the base triangle only
		constexpr int kX = kSize / 2;

		const auto drawOver = [&](PipelineHandle over) {
			return drawSteps(rhi(), target, pass, {
				{ basePipeline, fullScreen(kBaseColor) },
				{ over, topHalf(kOverColor) },
				});
		};

		// The picture the first pipeline paints, recorded before anything is destroyed, so the cases
		// below are comparisons against a known-good frame rather than against an expectation alone.
		const auto first = makePipeline(rhi(), pass, opaqueOptions);
		ASSERT_TRUE(first) << "the first pipeline could not be created";
		{
			const auto pixels = drawOver(first);
			ASSERT_FALSE(pixels.empty()) << "readback failed before any destroy";
			ASSERT_EQ(pixelAt(pixels, kX, kBaseOnlyY), kBase) << "the base draw did not lay down its colour";
			ASSERT_EQ(pixelAt(pixels, kX, kOverlapY), kOpaque) << "the first pipeline did not draw opaquely";
		}

		// Destroy and recreate on the same render pass. Both backends recycle handle slots, and
		// PipelineHandle is a bare id with no generation counter, so the second create lands on the
		// very id the first one gave up and the stale handle is byte-identical to the live one — a
		// caller holding the old handle silently addresses the new pipeline. That is why what is
		// asserted here is the picture and not the id: the blend state is the only difference between
		// the two pipelines, so a recycled slot that quietly kept the destroyed pipeline's program
		// would still bind, still draw, and paint kOpaque here.
		rhi().destroyGraphicsPipeline(first);
		const auto second = makePipeline(rhi(), pass, blendedOptions);
		ASSERT_TRUE(second) << "a pipeline could not be created on a render pass whose previous pipeline was destroyed";
		{
			const auto pixels = drawOver(second);
			ASSERT_FALSE(pixels.empty()) << "readback failed after destroy and recreate";
			EXPECT_EQ(pixelAt(pixels, kX, kBaseOnlyY), kBase)
				<< "destroying a pipeline broke the surviving one: the base draw no longer lands";
			EXPECT_EQ(pixelAt(pixels, kX, kOverlapY), kBlended)
				<< "the recreated pipeline painted " << (pixelAt(pixels, kX, kOverlapY) == kOpaque
					? "the destroyed pipeline's picture, so the recycled slot kept the old state"
					: "neither pipeline's picture");
		}

		// Double destroy. Shutdown paths double-free by accident often enough that this must not be a
		// crash — but "it did not crash" is also what a backend that recycled the slot twice would
		// report, and that free list would then hand the same id to two live pipelines. So the second
		// destroy is followed by a create and a draw: if the slot was released twice, this pipeline
		// shares an id with the next one and the picture stops being reproducible.
		rhi().destroyGraphicsPipeline(second);
		rhi().destroyGraphicsPipeline(second);
		{
			const auto third = makePipeline(rhi(), pass, opaqueOptions);
			ASSERT_TRUE(third) << "no pipeline could be created after a handle was destroyed twice";
			const auto pixels = drawOver(third);
			ASSERT_FALSE(pixels.empty()) << "readback failed after a double destroy";
			EXPECT_EQ(pixelAt(pixels, kX, kBaseOnlyY), kBase)
				<< "the double destroy took the surviving pipeline with it";
			EXPECT_EQ(pixelAt(pixels, kX, kOverlapY), kOpaque)
				<< "the pipeline created after a double destroy did not draw its own state";
			rhi().destroyGraphicsPipeline(third);
		}

		// Destroying a pipeline a previous, already submitted frame drew with. Vulkan may not free a
		// VkPipeline the GPU is still reading and VulkanRHI::destroyGraphicsPipeline frees inline, so
		// what is pinned here is the contract the engine actually relies on: the frame was submitted,
		// presented and waited for (drawSteps ends with waitIdle) before the destroy, and a later
		// frame must then be unaffected. OpenGL takes an extra step on this path — destroying the
		// currently bound pipeline also issues glUseProgram(0) and glBindVertexArray(0) — so the
		// following frame's first bind has to re-establish everything it needs, which is exactly what
		// the whole-image scan below checks.
		{
			const auto doomed = makePipeline(rhi(), pass, opaqueOptions);
			ASSERT_TRUE(doomed) << "pipeline creation failed";
			const auto pixels = drawOver(doomed);
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			// The frame that will still be in the backend's hands at the destroy below really drew
			// with this pipeline, and it was the last one bound in that frame.
			ASSERT_EQ(pixelAt(pixels, kX, kOverlapY), kOpaque) << "the doomed pipeline never drew";

			rhi().destroyGraphicsPipeline(doomed);

			// A whole frame afterwards, with nothing but the survivor: every pixel must be the base
			// draw's colour. Scanned rather than spot-checked because the failure this guards against
			// — a half-unbound program or vertex array left behind by the destroy — paints garbage
			// somewhere unpredictable, or nothing at all, rather than a specific wrong colour.
			const auto after = drawSteps(rhi(), target, pass, { { basePipeline, fullScreen(kBaseColor) } });
			ASSERT_FALSE(after.empty()) << "readback failed in the frame after the destroy";
			int wrong = 0;
			int firstWrongX = -1;
			int firstWrongY = -1;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					if (!(pixelAt(after, x, y) == kBase)) {
						if (wrong == 0) {
							firstWrongX = x;
							firstWrongY = y;
						}
						++wrong;
					}
				}
			}
			EXPECT_EQ(wrong, 0) << wrong
				<< " pixel(s) are not the surviving pipeline's colour, the first at (" << firstWrongX
				<< ", " << firstWrongY
				<< "): destroying a pipeline a previously submitted frame drew with left the next frame "
				   "unable to draw";
		}

		rhi().destroyGraphicsPipeline(basePipeline);
	}

	// A scissor rectangle that hangs off the edge of the target. The two backends reach their answer
	// by different routes, and neither test above walks either route: OpenGLRHI::execSetScissor takes
	// a `width == 0 || height == 0` shortcut before it flips and clamps, so a rectangle that is
	// non-zero as asked for but empty once clamped is the only input that gets past the shortcut and
	// into the clamp; VulkanRHI::cmdSetScissor has no shortcut at all and arrives at the same empty
	// rectangle through `min`/`max` on both corners.
	//
	// The partly-overlapping cases are the ones worth having. Shrinking an extent and moving an
	// origin are indistinguishable while a rectangle is fully on the target, and the two backends do
	// it differently — OpenGL pulls a negative origin up to zero and takes the difference out of the
	// extent, then leaves the far edges to GL, while Vulkan clamps both corners itself. If either got
	// it wrong the surviving part of the rectangle would slide towards the origin instead of staying
	// where it was asked for, and ImGui's clip rects — the editor's only caller — straddle the
	// viewport edge whenever a panel is dragged half off it, so this is a shape that arrives in
	// practice and not only in a test.
	//
	// Rectangles are in cmdSetScissor's own coordinates: top-left origin, signed x and y. The pixels
	// beside each are readback coordinates, the orientation
	// ScissorClipsToTheSameOffCentreRectangleOnBothBackends already pins on both backends.
	TEST_P(RhiBackendTest, ScissorRectangleOffTheTargetClampsInPlaceOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue clear against a red draw: a pixel that is not the clear colour was painted, and the
		// last case below paints some, so "nothing was painted" cannot be a dead draw.
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		const auto pipeline = makePipeline(rhi(), pass, PipelineOptions{});
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const Math::Vec4 kDrawColor{ 1.f, 0.f, 0.f, 1.f };
		const Rgba kDrawn{ 255, 0, 0, 255 };
		const Rgba kClear{ 0, 0, 255, 255 };

		struct Case {
			const char* label;
			ScissorRect rect;
			// The pixels the rectangle must paint, half-open in readback coordinates. An empty box
			// means the whole image must stay at the clear colour.
			int x0, y0, x1, y1;
		};
		const Case kCases[] = {
			// Past the far corner on both axes. Non-zero on paper, so the OpenGL shortcut cannot fire;
			// empty only because the clamp says so.
			{ "fully past the far corner", ScissorRect{ 100, 100, 4, 4 }, 0, 0, 0, 0 },
			// The other direction. cmdSetScissor takes signed x and y, so this is a legitimate ask, and
			// it is the case where OpenGL's "shrink the extent" clamp drives the width negative — a
			// backend that clamped the origin without touching the extent would paint a 4x4 block at
			// the corner instead of nothing.
			{ "fully past the near corner", ScissorRect{ -10, -10, 4, 4 }, 0, 0, 0, 0 },
			// Straddling the left edge: x runs -2..1, so only columns 0 and 1 survive, and they must
			// stay at columns 0 and 1. A rectangle that kept its full width after having its origin
			// clamped would reach column 3.
			{ "straddling the left edge", ScissorRect{ -2, 2, 4, 3 }, 0, 2, 2, 5 },
			// Straddling the top edge, which is the far edge once flipped: rows -2..1, so only rows 0
			// and 1 survive. OpenGL leaves this clamp to GL and Vulkan does it with a `min`, so it is a
			// different pair of code paths from the case above rather than the same one transposed.
			{ "straddling the top edge", ScissorRect{ 2, -2, 3, 4 }, 2, 0, 5, 2 },
			// The control, fully on the target: same target, pass, pipeline and triangle. If this
			// paints its nine pixels then the empty cases above were rejected by the scissor and not by
			// a draw that never ran.
			{ "fully on the target", ScissorRect{ 2, 2, 3, 3 }, 2, 2, 5, 5 },
		};

		for (const auto& testCase : kCases) {
			const auto pixels = drawSteps(rhi(), target, pass, {
				{ pipeline, fullScreen(kDrawColor), testCase.rect },
				});
			ASSERT_FALSE(pixels.empty()) << testCase.label << ": readback failed";

			// Every pixel, both ways round. A count of painted pixels alone would let a rectangle of
			// the right size in the wrong place through, which is precisely the failure a clamp that
			// moves the origin produces.
			int wrong = 0;
			int firstX = -1;
			int firstY = -1;
			int painted = 0;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					const bool inside = x >= testCase.x0 && x < testCase.x1
						&& y >= testCase.y0 && y < testCase.y1;
					const bool isDrawn = pixelAt(pixels, x, y) == kDrawn;
					if (isDrawn) {
						++painted;
					}
					const bool isClear = pixelAt(pixels, x, y) == kClear;
					if (isDrawn != inside || !(isDrawn || isClear)) {
						if (wrong == 0) {
							firstX = x;
							firstY = y;
						}
						++wrong;
					}
				}
			}
			const int expected = (testCase.x1 - testCase.x0) * (testCase.y1 - testCase.y0);
			EXPECT_EQ(wrong, 0) << "a scissor rectangle " << testCase.label << " ({" << testCase.rect.x
				<< ", " << testCase.rect.y << ", " << testCase.rect.width << ", " << testCase.rect.height
				<< "}) painted " << painted << " pixel(s) instead of " << expected
				<< "; " << wrong << " pixel(s) disagree, the first at (" << firstX << ", " << firstY
				<< "). Expected exactly x [" << testCase.x0 << ", " << testCase.x1 << ") by y ["
				<< testCase.y0 << ", " << testCase.y1 << "): a clamped rectangle keeps the position it "
				   "was asked for and loses only the part that is off the target";
		}
	}

	// PrimitiveTopology::LineStrip, the one enumerator whose meaning changed rather than moved. It used
	// to be LineLoop: OpenGL asked for GL_LINE_LOOP, which closes the outline with a segment from the
	// last vertex back to the first, while VulkanRHIVkUtil mapped it to
	// VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, which has no such segment — so the closing segment existed on
	// one backend only. It is now LineStrip on both, and closing a loop is the caller's job: repeat the
	// first vertex. Nothing but this test says so, and the absence of that closing segment is exactly
	// the half a strip test can be written without.
	//
	// Three vertices meeting at a right angle in the bottom-left corner, each on a pixel centre, so the
	// two segments run along a full column and a full row of centres and are covered under every
	// rasterisation rule. Both drawn probes sit mid-segment, away from the endpoints, where the
	// diamond-exit rule decides and both specs leave the answer implementation-defined. Nothing is
	// asserted about line width: the drawn probes are pixels a segment's centre must cover, and the
	// clear probes sit three pixels clear of every segment, so no defensible width reaches them.
	TEST_P(RhiBackendTest, LineStripJoinsConsecutiveVerticesWithoutClosingBackToTheFirstOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue clear against a white draw: the diagonal probes below are pixels the old GL_LINE_LOOP
		// mapping would have painted, so a backend that still closes the strip shows as a colour.
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		const Rgba kDrawn{ 255, 255, 255, 255 };
		const Rgba kClear{ 0, 0, 255, 255 };
		constexpr float kW = 1.f; // white in the colour slot: the pixel says only whether it was covered

		// (0, 0) -> (0, 7) -> (7, 7) in readback pixels. The first segment is column 0, the second is
		// row 7, and the segment a loop would add runs corner to corner along the y = x diagonal, whose
		// pixel centres it passes through exactly — so a closing segment cannot slip between the probes.
		const Vertex corner[3] = {
			{ pixelCentreX(0), pixelCentreY(0), kW, kW, kW, kW }, // top-left
			{ pixelCentreX(0), pixelCentreY(7), kW, kW, kW, kW }, // bottom-left, the right angle
			{ pixelCentreX(7), pixelCentreY(7), kW, kW, kW, kW }, // bottom-right
		};
		const auto vbo = makeVertexBuffer(rhi(), corner, 3);
		ASSERT_TRUE(vbo);

		// Mid-segment, four pixels from either endpoint.
		constexpr int kFirstSegmentX = 0;
		constexpr int kFirstSegmentY = 3;
		constexpr int kSecondSegmentX = 3;
		constexpr int kSecondSegmentY = 7;

		{
			const auto pipeline = makeVertexPipeline(rhi(), pass, PrimitiveTopology::LineStrip);
			ASSERT_TRUE(pipeline) << "a LineStrip pipeline could not be created";

			DrawStep step;
			step.pipeline = pipeline;
			step.vertices = vbo;
			step.vertexCount = 3;
			const auto pixels = drawSteps(rhi(), target, pass, { step });
			ASSERT_FALSE(pixels.empty()) << "LineStrip: readback failed";

			EXPECT_EQ(pixelAt(pixels, kFirstSegmentX, kFirstSegmentY), kDrawn)
				<< "the first segment is unpainted: the LineStrip draw produced nothing at all";
			// The half that separates a strip from Lines: under Lines the first two vertices are one
			// segment and the third is dropped, so this row stays at the clear colour. The control below
			// draws exactly that.
			EXPECT_EQ(pixelAt(pixels, kSecondSegmentX, kSecondSegmentY), kDrawn)
				<< "the second segment is unpainted: vertex 1 was not joined to vertex 2, so the three "
				   "vertices were grouped as independent pairs rather than as a strip";

			// The whole point. A line's width thickens it across its length, never along it, so these two
			// pixels are unreachable from either segment at any width — but they sit on the segment from
			// the last vertex back to the first, which a strip must not draw.
			EXPECT_EQ(pixelAt(pixels, 3, 3), kClear)
				<< "the diagonal from the last vertex back to the first was painted: the strip closed into "
				   "a loop, which is the old GL_LINE_LOOP behaviour Vulkan never had";
			EXPECT_EQ(pixelAt(pixels, 4, 4), kClear)
				<< "the diagonal from the last vertex back to the first was painted: the strip closed into "
				   "a loop";
		}

		// The control, same target, pass, vertices and probes with only the topology changed. "The second
		// segment was painted" means a strip only if Lines really leaves that pixel alone — otherwise the
		// assertion above would pass for a backend that ignored topology altogether.
		{
			const auto pipeline = makeVertexPipeline(rhi(), pass, PrimitiveTopology::Lines);
			ASSERT_TRUE(pipeline) << "a Lines pipeline could not be created";

			DrawStep step;
			step.pipeline = pipeline;
			step.vertices = vbo;
			step.vertexCount = 3;
			const auto pixels = drawSteps(rhi(), target, pass, { step });
			ASSERT_FALSE(pixels.empty()) << "Lines control: readback failed";

			EXPECT_EQ(pixelAt(pixels, kFirstSegmentX, kFirstSegmentY), kDrawn)
				<< "the control drew nothing, so the strip case above proves nothing";
			EXPECT_EQ(pixelAt(pixels, kSecondSegmentX, kSecondSegmentY), kClear)
				<< "Lines painted the second segment from an odd vertex out, so that probe cannot tell a "
				   "strip from a pair of independent lines";
		}
	}
}
