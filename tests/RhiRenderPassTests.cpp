#include "RhiTestFixture.h"

// Render passes and framebuffers: what a pass does to the attachment it is handed before the first
// draw of the pass reaches it. A load op is the one piece of pass state whose effect is invisible
// inside the pass that carries it -- it is only observable as the difference between what the
// previous pass stored and what the next one starts from -- so it needs two passes over one
// attachment to pin at all.
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

		// The clear colour is still set on a Load pass: it is the value the attachment would come back
		// at if the load op were quietly treated as a clear, and it matches nothing else in the test, so
		// a mistaken clear names itself instead of looking like a lost draw.
		RenderPassHandle makeLoadPass(RenderHardwareInterface& rhi, const Math::Vec4& clear) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = clear;
			return rhi.createRenderPass(desc);
		}

		PipelineHandle makePipeline(RenderHardwareInterface& rhi, RenderPassHandle pass) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.pushConstantSize = sizeof(TrianglePush);
			desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		FramebufferHandle makeFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass, TextureHandle target) {
			FramebufferDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.layers = 1;
			desc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = target;
			att.layer = UINT32_MAX;
			desc.colorAttachments = { att };
			return rhi.createFramebuffer(desc);
		}

		struct DrawStep {
			PipelineHandle pipeline;
			TrianglePush push;
		};

		// One pass, one frame, then wait. Each pass of the test has to be its own submitted frame:
		// the question being asked is what the *next* pass sees of what this one stored, and two passes
		// recorded into one command list would leave the attachment's contents between them a matter of
		// the backend's internal ordering rather than of the load op.
		bool runPass(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			const std::vector<DrawStep>& steps) {
			if (!fb) {
				return false;
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
				rhi.cmdBindPipeline(cmd, step.pipeline);
				rhi.cmdPushConstants(cmd, step.pipeline, ShaderStage::Vertex | ShaderStage::Fragment,
					&step.push, sizeof(step.push));
				rhi.cmdDraw(cmd, 3, 1, 0, 0);
			}
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
			return true;
		}

		std::vector<uint8_t> readTarget(RenderHardwareInterface& rhi, TextureHandle target) {
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

		TrianglePush fullScreen(const Math::Vec4& color) {
			TrianglePush push;
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { 3.f, -1.f, 0.f, 1.f };
			push.c = { -1.f, 3.f, 0.f, 1.f };
			push.color = color;
			return push;
		}

		// The positive-Y half of clip space, which readback presents as the top four rows.
		TrianglePush topHalf(const Math::Vec4& color) {
			TrianglePush push;
			push.a = { -1.f, 0.f, 0.f, 1.f };
			push.b = { 3.f, 0.f, 0.f, 1.f };
			push.c = { -1.f, 4.f, 0.f, 1.f };
			push.color = color;
			return push;
		}

		constexpr float unorm(int byteValue) { return static_cast<float>(byteValue) / 255.f; }

		// Four colours no two of which share a channel value, so any pixel names where it came from:
		// the first pass's clear, the first pass's draw, the second pass's draw, or the second pass's
		// clear. A mixed-up pair reads as the wrong colour, never as a coincidence.
		const Math::Vec4 kFirstClear{ unorm(16), unorm(32), unorm(48), unorm(64) };
		const Math::Vec4 kFirstDraw{ unorm(80), unorm(96), unorm(112), unorm(128) };
		const Math::Vec4 kSecondDraw{ unorm(144), unorm(160), unorm(176), unorm(192) };
		const Math::Vec4 kSecondClear{ unorm(208), unorm(224), unorm(240), unorm(255) };

		constexpr int kTopRows = kSize / 2;
	}

	// AttachmentLoadOp is the whole reason a multi-pass frame can exist: every pass after the first
	// one that targets the same attachment relies on Load meaning "what the last pass stored is still
	// there", and on Clear meaning "throw it away" no matter what was there. If either backend read a
	// Load as a clear, an overlay pass would erase the scene it was meant to draw on top of; if a
	// Clear leaked the previous contents, a target reused between frames would show last frame's
	// image through the gaps.
	TEST_P(RhiBackendTest, AttachmentLoadOpLoadPreservesContentsWhileClearReplacesThem) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);

		const auto firstPass = makeClearPass(rhi(), kFirstClear);
		ASSERT_TRUE(firstPass);
		const auto loadPass = makeLoadPass(rhi(), kFirstClear);
		ASSERT_TRUE(loadPass);
		const auto clearPass = makeClearPass(rhi(), kSecondClear);
		ASSERT_TRUE(clearPass);

		const auto firstPipeline = makePipeline(rhi(), firstPass);
		ASSERT_TRUE(firstPipeline);
		const auto loadPipeline = makePipeline(rhi(), loadPass);
		ASSERT_TRUE(loadPipeline);

		const auto firstFb = makeFramebuffer(rhi(), firstPass, target);
		ASSERT_TRUE(firstFb);
		const auto loadFb = makeFramebuffer(rhi(), loadPass, target);
		ASSERT_TRUE(loadFb);
		const auto clearFb = makeFramebuffer(rhi(), clearPass, target);
		ASSERT_TRUE(clearFb);

		ASSERT_TRUE(runPass(rhi(), firstPass, firstFb, { { firstPipeline, fullScreen(kFirstDraw) } }));
		{
			const auto pixels = readTarget(rhi(), target);
			ASSERT_FALSE(pixels.empty());
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					ASSERT_EQ(pixelAt(pixels, x, y), (Rgba{ 80, 96, 112, 128 }))
						<< "the first pass did not cover the attachment at " << x << "," << y;
				}
			}
		}

		ASSERT_TRUE(runPass(rhi(), loadPass, loadFb, { { loadPipeline, topHalf(kSecondDraw) } }));
		{
			const auto pixels = readTarget(rhi(), target);
			ASSERT_FALSE(pixels.empty());
			for (int y = 0; y < kSize; ++y) {
				const Rgba expected = y < kTopRows
					? Rgba{ 144, 160, 176, 192 }
					: Rgba{ 80, 96, 112, 128 };
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< (y < kTopRows
							? "the Load pass did not draw over the loaded contents at "
							: "the Load pass did not preserve the first pass's contents at ")
						<< x << "," << y;
				}
			}
		}

		// No draw at all, onto an attachment holding two different colours: whatever the Clear leaves
		// behind is the clear colour or it is a leak, and there is no draw in this pass that could be
		// mistaken for either.
		ASSERT_TRUE(runPass(rhi(), clearPass, clearFb, {}));
		{
			const auto pixels = readTarget(rhi(), target);
			ASSERT_FALSE(pixels.empty());
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), (Rgba{ 208, 224, 240, 255 }))
						<< "the Clear pass did not replace the previous contents at " << x << "," << y;
				}
			}
		}

		rhi().destroyFramebuffer(clearFb);
		rhi().destroyFramebuffer(loadFb);
		rhi().destroyFramebuffer(firstFb);
		rhi().destroyGraphicsPipeline(loadPipeline);
		rhi().destroyGraphicsPipeline(firstPipeline);
		rhi().destroyRenderPass(clearPass);
		rhi().destroyRenderPass(loadPass);
		rhi().destroyRenderPass(firstPass);
		rhi().destroyTexture(target);
	}

	namespace {

		RenderPassHandle makeDontCarePass(RenderHardwareInterface& rhi, const Math::Vec4& clear) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::DontCare;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = clear;
			return rhi.createRenderPass(desc);
		}

		const Math::Vec4 kStoreOpClear{ unorm(12), unorm(28), unorm(44), unorm(60) };
		const Math::Vec4 kStoreOpDraw{ unorm(76), unorm(92), unorm(108), unorm(124) };
	}

	// storeOp is the one attachment field whose Store side is a promise and whose DontCare side is only
	// a permission: after a DontCare pass the attachment's contents are undefined, so no value read back
	// here is owed by either backend. The expectations below are therefore *measured*, not guaranteed --
	// they say what these two backends do on a desktop GPU, where neither has a reason to throw the
	// pixels away: OpenGL never reads the field at all, and Vulkan hands VK_ATTACHMENT_STORE_OP_DONT_CARE
	// to an immediate renderer that has nowhere cheaper to put them than the image they are already in.
	// A tiled GPU would be within its rights to return anything, and the answer there is to widen this
	// expectation, not to call it a regression. The Store half of the test is the real contract, and it
	// is what makes the DontCare half readable: without a control saying the draw definitely lands, "the
	// colour is still there" would not distinguish a kept attachment from a pass that did nothing.
	TEST_P(RhiBackendTest, AttachmentStoreOpDontCareLeavesTheSameContentsAsStore) {
		const auto storeTarget = makeTarget(rhi());
		ASSERT_TRUE(storeTarget);
		const auto dontCareTarget = makeTarget(rhi());
		ASSERT_TRUE(dontCareTarget);

		const auto storePass = makeClearPass(rhi(), kStoreOpClear);
		ASSERT_TRUE(storePass);
		const auto dontCarePass = makeDontCarePass(rhi(), kStoreOpClear);
		ASSERT_TRUE(dontCarePass);

		const auto storePipeline = makePipeline(rhi(), storePass);
		ASSERT_TRUE(storePipeline);
		const auto dontCarePipeline = makePipeline(rhi(), dontCarePass);
		ASSERT_TRUE(dontCarePipeline);

		const auto storeFb = makeFramebuffer(rhi(), storePass, storeTarget);
		ASSERT_TRUE(storeFb);
		const auto dontCareFb = makeFramebuffer(rhi(), dontCarePass, dontCareTarget);
		ASSERT_TRUE(dontCareFb);

		ASSERT_TRUE(runPass(rhi(), storePass, storeFb, { { storePipeline, topHalf(kStoreOpDraw) } }));
		{
			const auto pixels = readTarget(rhi(), storeTarget);
			ASSERT_FALSE(pixels.empty());
			for (int y = 0; y < kSize; ++y) {
				const Rgba expected = y < kTopRows
					? Rgba{ 76, 92, 108, 124 }
					: Rgba{ 12, 28, 44, 60 };
				for (int x = 0; x < kSize; ++x) {
					ASSERT_EQ(pixelAt(pixels, x, y), expected)
						<< "the Store pass did not keep what it drew and cleared at " << x << "," << y;
				}
			}
		}

		ASSERT_TRUE(runPass(rhi(), dontCarePass, dontCareFb, { { dontCarePipeline, topHalf(kStoreOpDraw) } }));
		{
			const auto pixels = readTarget(rhi(), dontCareTarget);
			ASSERT_FALSE(pixels.empty());
			for (int y = 0; y < kSize; ++y) {
				const Rgba expected = y < kTopRows
					? Rgba{ 76, 92, 108, 124 }
					: Rgba{ 12, 28, 44, 60 };
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< "the DontCare pass left something other than the measured contents at "
						<< x << "," << y;
				}
			}
		}

		rhi().destroyFramebuffer(dontCareFb);
		rhi().destroyFramebuffer(storeFb);
		rhi().destroyGraphicsPipeline(dontCarePipeline);
		rhi().destroyGraphicsPipeline(storePipeline);
		rhi().destroyRenderPass(dontCarePass);
		rhi().destroyRenderPass(storePass);
		rhi().destroyTexture(dontCareTarget);
		rhi().destroyTexture(storeTarget);
	}

	namespace {

		constexpr uint32_t kMrtCount = 4;

		// Six colours, no two sharing a channel value anywhere: each attachment's clear and each
		// shader output names itself, so a swapped pair reads as another attachment's colour rather
		// than as something plausible.
		const Math::Vec4 kMrtClears[kMrtCount] = {
			{ unorm(10), unorm(30), unorm(50), unorm(70) },
			{ unorm(90), unorm(110), unorm(130), unorm(150) },
			{ unorm(170), unorm(190), unorm(210), unorm(230) },
			{ unorm(5), unorm(25), unorm(45), unorm(65) },
		};

		// Must match rhi_test_mrt.fs, which writes these to locations 0 through 3.
		const Rgba kMrtWrites[kMrtCount] = {
			{ 20, 40, 60, 80 },
			{ 100, 120, 140, 160 },
			{ 180, 200, 220, 240 },
			{ 15, 35, 55, 75 },
		};

		RenderPassHandle makeMrtPass(RenderHardwareInterface& rhi) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = kMrtCount;
			for (uint32_t i = 0; i < kMrtCount; ++i) {
				desc.colorAttachments[i].loadOp = AttachmentLoadOp::Clear;
				desc.colorAttachments[i].storeOp = AttachmentStoreOp::Store;
				desc.colorAttachments[i].format = TextureFormat::RGBA8;
				desc.colorAttachments[i].clearColor = kMrtClears[i];
			}
			return rhi.createRenderPass(desc);
		}

		PipelineHandle makeMrtPipeline(RenderHardwareInterface& rhi, RenderPassHandle pass) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.pushConstantSize = sizeof(TrianglePush);
			desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_mrt.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		FramebufferHandle makeMrtFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass,
			const TextureHandle (&targets)[kMrtCount]) {
			FramebufferDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.layers = 1;
			desc.renderPass = pass;
			for (const auto target : targets) {
				FramebufferAttachmentDesc att;
				att.texture = target;
				att.layer = UINT32_MAX;
				desc.colorAttachments.push_back(att);
			}
			return rhi.createFramebuffer(desc);
		}
	}

	// Attachment order is the contract a deferred renderer is built on: a g-buffer pass writes albedo,
	// normals and material to three targets in one draw, and nothing downstream can tell that two of
	// them were swapped -- the lighting would simply be wrong. So the test asks two things at once.
	// The covered half pins which output location reached which texture, and the uncovered half pins
	// that every attachment was cleared, not just attachment 0: a backend that clears only the first
	// target leaves the others at whatever the allocation held, which the covered half alone would
	// never notice.
	TEST_P(RhiBackendTest, EveryColorAttachmentReceivesItsOwnFragmentOutput) {
		TextureHandle targets[kMrtCount]{};
		for (auto& target : targets) {
			target = makeTarget(rhi());
			ASSERT_TRUE(target);
		}

		const auto pass = makeMrtPass(rhi());
		ASSERT_TRUE(pass);
		const auto pipeline = makeMrtPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline);
		const auto fb = makeMrtFramebuffer(rhi(), pass, targets);
		ASSERT_TRUE(fb);

		// The push colour is dead here -- the fragment shader ignores it and writes its own three
		// constants -- but the vertex stage still reads the corners out of the same block.
		ASSERT_TRUE(runPass(rhi(), pass, fb, { { pipeline, topHalf({ 0.f, 0.f, 0.f, 1.f }) } }));

		for (uint32_t i = 0; i < kMrtCount; ++i) {
			const auto pixels = readTarget(rhi(), targets[i]);
			ASSERT_FALSE(pixels.empty()) << "attachment " << i << " did not read back";
			const Rgba drawn = kMrtWrites[i];
			const Rgba cleared{
				static_cast<uint8_t>(kMrtClears[i].x * 255.f + 0.5f),
				static_cast<uint8_t>(kMrtClears[i].y * 255.f + 0.5f),
				static_cast<uint8_t>(kMrtClears[i].z * 255.f + 0.5f),
				static_cast<uint8_t>(kMrtClears[i].w * 255.f + 0.5f),
			};
			for (int y = 0; y < kSize; ++y) {
				const Rgba expected = y < kTopRows ? drawn : cleared;
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< (y < kTopRows
							? "attachment " + std::to_string(i) + " did not get the fragment output at location "
								+ std::to_string(i) + " at "
							: "attachment " + std::to_string(i) + " lost its own clear colour outside the draw at ")
						<< x << "," << y;
				}
			}
		}

		rhi().destroyFramebuffer(fb);
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyRenderPass(pass);
		for (const auto target : targets) {
			rhi().destroyTexture(target);
		}
	}

	namespace {

		// Four baked clears, four overrides and a tail sentinel: thirty-six byte values, none of
		// them repeated anywhere, so every pixel read back names which of the three sources produced
		// it instead of being consistent with two of them.
		const Math::Vec4 kBakedClears[kMrtCount] = {
			{ unorm(4), unorm(8), unorm(12), unorm(16) },
			{ unorm(20), unorm(24), unorm(28), unorm(32) },
			{ unorm(36), unorm(40), unorm(44), unorm(48) },
			{ unorm(116), unorm(120), unorm(124), unorm(128) },
		};
		const Rgba kBakedExpected[kMrtCount] = {
			{ 4, 8, 12, 16 },
			{ 20, 24, 28, 32 },
			{ 36, 40, 44, 48 },
			{ 116, 120, 124, 128 },
		};

		const RhiVec4 kOverrideClears[kMrtCount] = {
			{ unorm(52), unorm(56), unorm(60), unorm(64) },
			{ unorm(68), unorm(72), unorm(76), unorm(80) },
			{ unorm(84), unorm(88), unorm(92), unorm(96) },
			{ unorm(132), unorm(136), unorm(140), unorm(144) },
		};
		const Rgba kOverrideExpected[kMrtCount] = {
			{ 52, 56, 60, 64 },
			{ 68, 72, 76, 80 },
			{ 84, 88, 92, 96 },
			{ 132, 136, 140, 144 },
		};

		// What sits past the entries a caller meant to supply. See the short-array half of the test.
		const Math::Vec4 kTailSentinel{ unorm(100), unorm(104), unorm(108), unorm(112) };
		const Rgba kTailExpected{ 100, 104, 108, 112 };

		// readbackTexture quantises depth to 16 bits and packs it as R=high, G=low, B=high, A=255, so
		// 0.25 is 0x4000 and 0.75 is 0xBFFF. Both are far from a quantisation boundary, which matters
		// because the two backends store this format differently (32-bit unorm on OpenGL,
		// VK_FORMAT_D32_SFLOAT on Vulkan). Same pair the format matrix in RhiTextureTests.cpp uses.
		constexpr float kBakedDepth = 0.25f;
		constexpr float kOverrideDepth = 0.75f;
		const Rgba kBakedDepthPacked{ 64, 0, 64, 255 };
		const Rgba kOverrideDepthPacked{ 191, 255, 191, 255 };

		TextureHandle makeDepthTarget(RenderHardwareInterface& rhi) {
			TextureDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.depth = 1;
			desc.arrayLayers = 1;
			desc.dimension = TextureDimension::Tex2D;
			desc.format = TextureFormat::DEPTH_COMPONENT32;
			desc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled
				| TextureUsage::TransferSrc;
			return rhi.createTexture(desc);
		}

		RenderPassHandle makeOverridePass(RenderHardwareInterface& rhi) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = kMrtCount;
			for (uint32_t i = 0; i < kMrtCount; ++i) {
				desc.colorAttachments[i].loadOp = AttachmentLoadOp::Clear;
				desc.colorAttachments[i].storeOp = AttachmentStoreOp::Store;
				desc.colorAttachments[i].format = TextureFormat::RGBA8;
				desc.colorAttachments[i].clearColor = kBakedClears[i];
			}
			desc.hasDepthStencil = true;
			desc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
			desc.depthAttachment.storeOp = AttachmentStoreOp::Store;
			desc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
			desc.depthAttachment.clearDepth = kBakedDepth;
			return rhi.createRenderPass(desc);
		}

		FramebufferHandle makeOverrideFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass,
			const TextureHandle (&targets)[kMrtCount], TextureHandle depthTarget) {
			FramebufferDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.layers = 1;
			desc.renderPass = pass;
			for (const auto target : targets) {
				FramebufferAttachmentDesc att;
				att.texture = target;
				att.layer = UINT32_MAX;
				desc.colorAttachments.push_back(att);
			}
			FramebufferAttachmentDesc depthAtt;
			depthAtt.texture = depthTarget;
			depthAtt.layer = UINT32_MAX;
			desc.depthStencilAttachment = depthAtt;
			return rhi.createFramebuffer(desc);
		}

		void runClearOnlyPass(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			const RhiVec4* overrideColors, const float* overrideDepth) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			begin.overrideClearColors = overrideColors;
			begin.overrideClearDepth = overrideDepth;
			rhi.cmdBeginRenderPass(cmd, begin);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		void expectUniform(RenderHardwareInterface& rhi, TextureHandle target, const Rgba& expected,
			const std::string& what) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			ASSERT_TRUE(rhi.readbackTexture(target, pixels, w, h)) << what << ": readback refused";
			ASSERT_EQ(pixels.size(), static_cast<size_t>(kSize) * kSize * 4u) << what;
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					ASSERT_EQ(pixelAt(pixels, x, y), expected) << what << " at " << x << "," << y;
				}
			}
		}
	}

	// A render pass is created once and begun many times, so the clear values baked into it are a
	// default, not a constant: a shadow pass reused per light, or a viewport whose background follows
	// a setting, has to be able to say "this time, clear to something else" without building a second
	// pass object. That is what overrideClearColors and overrideClearDepth are for, and the contract
	// has three halves, all pinned here with no draw at all -- the clear is the entire write, so
	// nothing about a pipeline, a depth compare or a fragment output can reach the value under test.
	//
	// The first is precedence: with the pointers null the baked values land, with them set the
	// overridden ones do.
	//
	// The second is scope. Both backends read the pointers only while recording the begin -- OpenGL
	// copies the values into the recorded command at cmdBeginRenderPass (OpenGLRHI.cpp:1352-1373),
	// Vulkan builds its VkClearValue array there and hands it straight to vkCmdBeginRenderPass
	// (VulkanRHIPipelines.cpp:1342-1355) -- so an override binds to one begin and the next begin
	// without one must fall back to the pass. A backend that instead wrote the override into the
	// pass object would leave every later user of that pass clearing to someone else's colour, and
	// no single-begin test could see it.
	//
	// The third is the array's length, and it is the sharp edge. The field is documented as
	// "size >= colorAttachmentCount" and both backends take that literally: OpenGL copies
	// colorAttachmentCount entries out of it (OpenGLRHI.cpp:1359-1363) and Vulkan reads
	// overrideClearColors[i] for every i in the same range (VulkanRHIPipelines.cpp:1346). Neither
	// has a per-entry fallback and neither is told how long the array actually is, so supplying
	// fewer entries than the pass has attachments is an out-of-bounds read, not a partial override.
	// The third begin below measures exactly that, without committing the test to undefined
	// behaviour: the array is full length, but only entry 0 is a value a caller would have meant to
	// supply and entries 1 and 2 hold a sentinel standing in for whatever memory follows a short
	// array. The attachments coming back as the sentinel rather than as their baked clears is the
	// finding -- there is no "supplies fewer entries" behaviour to rely on, and a caller who tries
	// it reads past the end of their own buffer on both backends.
	TEST_P(RhiBackendTest, PerBeginClearOverridesBeatTheValuesBakedIntoTheRenderPass) {
		TextureHandle targets[kMrtCount]{};
		for (auto& target : targets) {
			target = makeTarget(rhi());
			ASSERT_TRUE(target);
		}
		const auto depthTarget = makeDepthTarget(rhi());
		ASSERT_TRUE(depthTarget);

		const auto pass = makeOverridePass(rhi());
		ASSERT_TRUE(pass);
		const auto fb = makeOverrideFramebuffer(rhi(), pass, targets, depthTarget);
		ASSERT_TRUE(fb);

		runClearOnlyPass(rhi(), pass, fb, nullptr, nullptr);
		for (uint32_t i = 0; i < kMrtCount; ++i) {
			ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), targets[i], kBakedExpected[i],
				"no override: attachment " + std::to_string(i) + " did not get the pass's baked clear colour"));
		}
		ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), depthTarget, kBakedDepthPacked,
			"no override: the depth attachment did not get the pass's baked clearDepth"));

		runClearOnlyPass(rhi(), pass, fb, kOverrideClears, &kOverrideDepth);
		for (uint32_t i = 0; i < kMrtCount; ++i) {
			ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), targets[i], kOverrideExpected[i],
				"override: attachment " + std::to_string(i) + " kept the baked clear colour instead of "
				"taking the one this begin supplied"));
		}
		ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), depthTarget, kOverrideDepthPacked,
			"override: the depth attachment kept the baked clearDepth instead of taking the one this "
			"begin supplied"));

		const RhiVec4 shortArray[kMrtCount] = { kOverrideClears[0], kTailSentinel, kTailSentinel, kTailSentinel };
		runClearOnlyPass(rhi(), pass, fb, shortArray, nullptr);
		ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), targets[0], kOverrideExpected[0],
			"short array: attachment 0, the one entry actually supplied, did not take its override"));
		for (uint32_t i = 1; i < kMrtCount; ++i) {
			ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), targets[i], kTailExpected,
				"short array: attachment " + std::to_string(i) + " answered with something other than "
				"the memory past the supplied entries â€” if it fell back to the baked clear instead, a "
				"backend has grown a per-entry fallback and this test should become that contract"));
		}
		// The depth pointer was null this time while the colour one was not, so this also pins that
		// the two overrides are independent.
		ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), depthTarget, kBakedDepthPacked,
			"short array: a null overrideClearDepth alongside a non-null overrideClearColors did not "
			"fall back to the baked clearDepth"));

		runClearOnlyPass(rhi(), pass, fb, nullptr, nullptr);
		for (uint32_t i = 0; i < kMrtCount; ++i) {
			ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), targets[i], kBakedExpected[i],
				"after an override: attachment " + std::to_string(i) + " did not fall back to the "
				"baked clear colour, so the override outlived the begin it was given to"));
		}
		ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), depthTarget, kBakedDepthPacked,
			"after an override: the depth attachment did not fall back to the baked clearDepth, so the "
			"override outlived the begin it was given to"));

		rhi().destroyFramebuffer(fb);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(depthTarget);
		for (const auto target : targets) {
			rhi().destroyTexture(target);
		}
	}

	namespace {

		const Math::Vec4 kBothClear{ unorm(24), unorm(56), unorm(88), unorm(120) };
		const Math::Vec4 kBothDraw{ unorm(152), unorm(184), unorm(216), unorm(248) };
		const Rgba kBothClearExpected{ 24, 56, 88, 120 };
		const Rgba kBothDrawExpected{ 152, 184, 216, 248 };

		constexpr float kBothDrawZ = -0.5f;
		constexpr int kBothDrawStored = 16384;
		constexpr int kBothClearStored = 65535;
		// The low byte of a 16-bit readback carries whatever the two formats rounded to, and they are
		// not the same format underneath (32-bit unorm on OpenGL, D32_SFLOAT on Vulkan). Both in fact
		// land on these two values exactly -- 0.25 and 1.0 are representable in either format and
		// quantise to 16 bits without loss -- so the tolerance is the suite's usual Â±512 kept for the
		// formats' sake, not for a measured spread. The two values sit 49151 apart, so it still cannot
		// let a wrong depth pass for a right one.
		constexpr int kBothTolerance = 512;

		RenderPassHandle makeColorAndDepthPass(RenderHardwareInterface& rhi) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = kBothClear;
			desc.hasDepthStencil = true;
			desc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
			desc.depthAttachment.storeOp = AttachmentStoreOp::Store;
			desc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
			desc.depthAttachment.clearDepth = 1.f;
			return rhi.createRenderPass(desc);
		}

		PipelineHandle makeDepthTestingPipeline(RenderHardwareInterface& rhi, RenderPassHandle pass) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { true, true, CompareOp::Less };
			desc.blend = { false };
			desc.pushConstantSize = sizeof(TrianglePush);
			desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		FramebufferHandle makeColorAndDepthFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass,
			TextureHandle color, TextureHandle depth) {
			FramebufferDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.layers = 1;
			desc.renderPass = pass;
			FramebufferAttachmentDesc colorAtt;
			colorAtt.texture = color;
			colorAtt.layer = UINT32_MAX;
			desc.colorAttachments = { colorAtt };
			FramebufferAttachmentDesc depthAtt;
			depthAtt.texture = depth;
			depthAtt.layer = UINT32_MAX;
			desc.depthStencilAttachment = depthAtt;
			return rhi.createFramebuffer(desc);
		}

		// The same constant z on all three corners, so the depth the triangle leaves behind is that
		// value everywhere it covers and no rasteriser's interpolation rule enters the comparison.
		TrianglePush topHalfAtDepth(const Math::Vec4& color, float z) {
			TrianglePush push;
			push.a = { -1.f, 0.f, z, 1.f };
			push.b = { 3.f, 0.f, z, 1.f };
			push.c = { -1.f, 4.f, z, 1.f };
			push.color = color;
			return push;
		}

		int depthValueAt(const std::vector<uint8_t>& pixels, int x, int y) {
			const auto texel = pixelAt(pixels, x, y);
			return (static_cast<int>(texel.r) << 8) | static_cast<int>(texel.g);
		}
	}

	// Both attachments of one pass, stored and read back together. Every pass in the engine that draws
	// geometry -- the depth prepass, the g-buffer, the forward passes behind them -- writes colour and
	// depth in the same pass and hands both on: the g-buffer's colour targets feed lighting while its
	// depth feeds the passes that draw against it afterwards. Nothing downstream could tell that the
	// depth half was dropped; the lighting would simply be lit, and the next pass would test against a
	// buffer still holding its clear. What the rest of the suite has pinned does not cover this. The
	// multiple-colour-attachment test stores three colour targets and no depth; the two depth tests in
	// RhiPipelineStateTests read the depth attachment but keep their colour as the thing under test,
	// and the override test here clears both without drawing at all. So this is the first assertion
	// that a *draw* lands in both attachments of one pass at once, and that both survive to readback.
	//
	// One pass, one draw covering half the target, and four quantities checked against each other: the
	// drawn colour where the triangle covered, the clear colour where it did not, the triangle's window
	// depth under the covered half and the cleared far depth under the rest. The uncovered half is what
	// makes the covered half readable -- without it, a depth attachment reading 16384 everywhere would
	// be indistinguishable from one the backend had cleared to the wrong value.
	//
	// Depth is window depth, (clip z + 1) / 2, which ClipSpaceZeroDepthStoresMidRangeOnBothBackends
	// pins as the OpenGL [-1, 1] mapping on both backends, and readback quantises it to 16 bits as
	// R = high byte, G = low byte.
	TEST_P(RhiBackendTest, OnePassStoresBothItsColorAndItsDepthAttachment) {
		const auto color = makeTarget(rhi());
		ASSERT_TRUE(color);
		const auto depth = makeDepthTarget(rhi());
		ASSERT_TRUE(depth);

		const auto pass = makeColorAndDepthPass(rhi());
		ASSERT_TRUE(pass);
		const auto pipeline = makeDepthTestingPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline);
		const auto fb = makeColorAndDepthFramebuffer(rhi(), pass, color, depth);
		ASSERT_TRUE(fb);

		ASSERT_TRUE(runPass(rhi(), pass, fb, { { pipeline, topHalfAtDepth(kBothDraw, kBothDrawZ) } }));

		const auto colorPixels = readTarget(rhi(), color);
		ASSERT_EQ(colorPixels.size(), static_cast<size_t>(kSize) * kSize * 4u)
			<< "the colour attachment did not read back";
		const auto depthPixels = readTarget(rhi(), depth);
		ASSERT_EQ(depthPixels.size(), static_cast<size_t>(kSize) * kSize * 4u)
			<< "the depth attachment did not read back";

		for (int y = 0; y < kSize; ++y) {
			const bool covered = y < kTopRows;
			const Rgba expectedColor = covered ? kBothDrawExpected : kBothClearExpected;
			const int expectedDepth = covered ? kBothDrawStored : kBothClearStored;
			for (int x = 0; x < kSize; ++x) {
				EXPECT_EQ(pixelAt(colorPixels, x, y), expectedColor)
					<< (covered
						? "the colour attachment did not keep the draw at "
						: "the colour attachment did not keep its clear colour outside the draw at ")
					<< x << "," << y;
				EXPECT_NEAR(depthValueAt(depthPixels, x, y), expectedDepth, kBothTolerance)
					<< (covered
						? "the depth attachment did not keep the draw's depth at "
						: "the depth attachment did not keep its cleared depth outside the draw at ")
					<< x << "," << y;
			}
		}

		rhi().destroyFramebuffer(fb);
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(depth);
		rhi().destroyTexture(color);
	}

	namespace {

		constexpr int kBlitHalfSize = kSize / 2;

		// R climbs with the column and G climbs with the row, so the pattern is asymmetric along both
		// axes at once: a blit that mirrored either one hands back a different colour rather than the
		// same gradient read the other way round. B and A are constant markers.
		Rgba blitSourceTexel(int col, int row) {
			return {
				static_cast<uint8_t>(16 + 16 * col),
				static_cast<uint8_t>(16 + 16 * row),
				8,
				255,
			};
		}

		// A 2:1 downscale puts each destination texel centre exactly on the boundary between the two
		// source texels it covers, in both axes, so the bilinear weights are 0.5/0.5 and the result is
		// the plain mean of the 2x2 block. The gradient steps by 16, so that mean is a whole byte and
		// there is nothing left for a driver to round differently.
		Rgba blitLinearHalfTexel(int col, int row) {
			return {
				static_cast<uint8_t>(24 + 32 * col),
				static_cast<uint8_t>(24 + 32 * row),
				8,
				255,
			};
		}

		std::vector<uint8_t> blitSourceUpload() {
			std::vector<uint8_t> data(static_cast<size_t>(kSize) * kSize * 4u);
			for (int row = 0; row < kSize; ++row) {
				for (int col = 0; col < kSize; ++col) {
					const Rgba texel = blitSourceTexel(col, row);
					const size_t offset = (static_cast<size_t>(row) * kSize + col) * 4u;
					data[offset] = texel.r;
					data[offset + 1] = texel.g;
					data[offset + 2] = texel.b;
					data[offset + 3] = texel.a;
				}
			}
			return data;
		}

		// Indexed by texture row, not by image row. readbackTexture hands texture row r back as image
		// row height-1-r, and a blit is defined on the texture, so that flip is undone here once and
		// every expectation below is written in the rows the blit itself works in. Without this the
		// readback flip and a genuine blit flip would compose into one number and neither could be
		// read out of the other.
		Rgba texelAt(const std::vector<uint8_t>& pixels, int width, int height, int col, int row) {
			const size_t offset = (static_cast<size_t>(height - 1 - row) * width + col) * 4u;
			if (offset + 3 >= pixels.size()) {
				return {};
			}
			return { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
		}

		TextureHandle makeSizedColorTarget(RenderHardwareInterface& rhi, int width, int height,
			const void* data) {
			TextureDesc desc;
			desc.width = static_cast<uint32_t>(width);
			desc.height = static_cast<uint32_t>(height);
			desc.depth = 1;
			desc.arrayLayers = 1;
			desc.dimension = TextureDimension::Tex2D;
			desc.format = TextureFormat::RGBA8;
			desc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled
				| TextureUsage::TransferSrc | TextureUsage::TransferDst;
			return rhi.createTexture(desc, data);
		}

		FramebufferHandle makeSizedColorFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass,
			int width, int height, TextureHandle color) {
			FramebufferDesc desc;
			desc.width = static_cast<uint32_t>(width);
			desc.height = static_cast<uint32_t>(height);
			desc.layers = 1;
			desc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = color;
			att.layer = UINT32_MAX;
			desc.colorAttachments = { att };
			return rhi.createFramebuffer(desc);
		}

		RenderPassHandle makeBlitDepthPass(RenderHardwareInterface& rhi, const Math::Vec4& clearColor,
			float clearDepth) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = clearColor;
			desc.hasDepthStencil = true;
			desc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
			desc.depthAttachment.storeOp = AttachmentStoreOp::Store;
			desc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
			desc.depthAttachment.clearDepth = clearDepth;
			return rhi.createRenderPass(desc);
		}

		// No render pass around it: a blit is a transfer, and Vulkan forbids vkCmdBlitImage inside a
		// render pass instance, so the frame carries the blit and nothing else.
		void runBlit(RenderHardwareInterface& rhi, FramebufferHandle src, FramebufferHandle dst,
			BlitMask mask, BlitFilter filter) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			rhi.cmdBlit(cmd, src, dst, mask, filter);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		std::vector<uint8_t> readSized(RenderHardwareInterface& rhi, TextureHandle tex, int width,
			int height) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			if (!rhi.readbackTexture(tex, pixels, w, h) || w != width || h != height) {
				return {};
			}
			return pixels;
		}

		const Math::Vec4 kBlitDepthDstClear{ unorm(36), unorm(68), unorm(100), unorm(132) };
		const Rgba kBlitDepthDstClearExpected{ 36, 68, 100, 132 };
		constexpr float kBlitDepthDstClearDepth = 0.5f;
		constexpr int kBlitDepthDstStored = 32768;
	}

	// cmdBlit is how one framebuffer's contents reach another without a draw: the glass pass copies the
	// scene it is about to refract through (GlassPass.cpp:390), and the present path scales the render
	// target onto the backbuffer (OpenGLRHI.cpp:1157, 1233). Neither the rectangles nor the orientation
	// are in the caller's hands -- the signature takes two framebuffers and nothing else
	// (RenderHardwareInterface.h:554), so "the whole source into the whole destination" is the entire
	// contract and the two backends have to mean the same thing by it. They arrive at it from opposite
	// conventions: OpenGL issues glBlitFramebuffer(0,0,srcW,srcH -> 0,0,dstW,dstH) in a space whose
	// origin is the bottom-left of the attachment, Vulkan issues vkCmdBlitImage over the same offsets in
	// image space, whose origin is the top-left. Both therefore map texture row 0 to texture row 0, but
	// that is a coincidence of two flips cancelling, not something the signature says, and a copy that
	// came back upside down on one backend would show up as a refraction sampling the wrong half of the
	// screen rather than as an error.
	//
	// Four blits out of one source. The first is 1:1 and pins orientation: the source gradient climbs in
	// both axes, so a mirrored copy reads as a different colour everywhere except the middle. The next
	// two halve the size with each filter -- Nearest picking one texel of each 2x2 block, Linear
	// averaging all four -- which is the only place the scaling rule is observable at all, since a 1:1
	// blit gives both filters the same answer. The last carries depth instead of colour through the same
	// call, on framebuffers that also hold colour, so the mask is pinned as selecting an aspect rather
	// than as being ignored.
	//
	// Every expectation is written in texture rows; see texelAt for why that is not the same as the rows
	// readback hands over.
	TEST_P(RhiBackendTest, CmdBlitCopiesColorAndDepthUnflippedAndDownscalesByItsFilter) {
		const auto upload = blitSourceUpload();
		const auto source = makeSizedColorTarget(rhi(), kSize, kSize, upload.data());
		ASSERT_TRUE(source);
		const auto copy = makeSizedColorTarget(rhi(), kSize, kSize, nullptr);
		ASSERT_TRUE(copy);
		const auto nearestHalf = makeSizedColorTarget(rhi(), kBlitHalfSize, kBlitHalfSize, nullptr);
		ASSERT_TRUE(nearestHalf);
		const auto linearHalf = makeSizedColorTarget(rhi(), kBlitHalfSize, kBlitHalfSize, nullptr);
		ASSERT_TRUE(linearHalf);

		const auto colorPass = makeClearPass(rhi(), kFirstClear);
		ASSERT_TRUE(colorPass);
		const auto sourceFb = makeSizedColorFramebuffer(rhi(), colorPass, kSize, kSize, source);
		ASSERT_TRUE(sourceFb);
		const auto copyFb = makeSizedColorFramebuffer(rhi(), colorPass, kSize, kSize, copy);
		ASSERT_TRUE(copyFb);
		const auto nearestHalfFb = makeSizedColorFramebuffer(rhi(), colorPass, kBlitHalfSize,
			kBlitHalfSize, nearestHalf);
		ASSERT_TRUE(nearestHalfFb);
		const auto linearHalfFb = makeSizedColorFramebuffer(rhi(), colorPass, kBlitHalfSize,
			kBlitHalfSize, linearHalf);
		ASSERT_TRUE(linearHalfFb);

		// The control for the three colour blits: if the source did not arrive in the texture the way
		// this test believes it did, every expectation after it would be measuring the upload.
		{
			const auto pixels = readSized(rhi(), source, kSize, kSize);
			ASSERT_FALSE(pixels.empty()) << "the blit source did not read back at " << kSize << "x" << kSize;
			for (int row = 0; row < kSize; ++row) {
				for (int col = 0; col < kSize; ++col) {
					ASSERT_EQ(texelAt(pixels, kSize, kSize, col, row), blitSourceTexel(col, row))
						<< "the source gradient is not in the texture at column " << col << ", row " << row;
				}
			}
		}

		runBlit(rhi(), sourceFb, copyFb, BlitMask::Color, BlitFilter::Nearest);
		{
			const auto pixels = readSized(rhi(), copy, kSize, kSize);
			ASSERT_FALSE(pixels.empty()) << "the 1:1 colour blit destination did not read back";
			for (int row = 0; row < kSize; ++row) {
				for (int col = 0; col < kSize; ++col) {
					EXPECT_EQ(texelAt(pixels, kSize, kSize, col, row), blitSourceTexel(col, row))
						<< "the 1:1 colour blit did not land source column " << col << ", row " << row
						<< " on the destination texel of the same name";
				}
			}
		}

		runBlit(rhi(), sourceFb, nearestHalfFb, BlitMask::Color, BlitFilter::Nearest);
		{
			const auto pixels = readSized(rhi(), nearestHalf, kBlitHalfSize, kBlitHalfSize);
			ASSERT_FALSE(pixels.empty()) << "the Nearest downscale destination did not read back";
			for (int row = 0; row < kBlitHalfSize; ++row) {
				for (int col = 0; col < kBlitHalfSize; ++col) {
					EXPECT_EQ(texelAt(pixels, kBlitHalfSize, kBlitHalfSize, col, row),
						blitSourceTexel(2 * col + 1, 2 * row + 1))
						<< "the Nearest downscale chose a different source texel for column " << col
						<< ", row " << row;
				}
			}
		}

		runBlit(rhi(), sourceFb, linearHalfFb, BlitMask::Color, BlitFilter::Linear);
		{
			const auto pixels = readSized(rhi(), linearHalf, kBlitHalfSize, kBlitHalfSize);
			ASSERT_FALSE(pixels.empty()) << "the Linear downscale destination did not read back";
			for (int row = 0; row < kBlitHalfSize; ++row) {
				for (int col = 0; col < kBlitHalfSize; ++col) {
					EXPECT_EQ(texelAt(pixels, kBlitHalfSize, kBlitHalfSize, col, row),
						blitLinearHalfTexel(col, row))
						<< "the Linear downscale did not average the 2x2 block at column " << col
						<< ", row " << row;
				}
			}
		}

		const auto depthSourceColor = makeTarget(rhi());
		ASSERT_TRUE(depthSourceColor);
		const auto depthSource = makeDepthTarget(rhi());
		ASSERT_TRUE(depthSource);
		const auto depthDestColor = makeTarget(rhi());
		ASSERT_TRUE(depthDestColor);
		const auto depthDest = makeDepthTarget(rhi());
		ASSERT_TRUE(depthDest);

		const auto depthSourcePass = makeColorAndDepthPass(rhi());
		ASSERT_TRUE(depthSourcePass);
		const auto depthDestPass = makeBlitDepthPass(rhi(), kBlitDepthDstClear, kBlitDepthDstClearDepth);
		ASSERT_TRUE(depthDestPass);
		const auto depthPipeline = makeDepthTestingPipeline(rhi(), depthSourcePass);
		ASSERT_TRUE(depthPipeline);
		const auto depthSourceFb = makeColorAndDepthFramebuffer(rhi(), depthSourcePass, depthSourceColor,
			depthSource);
		ASSERT_TRUE(depthSourceFb);
		const auto depthDestFb = makeColorAndDepthFramebuffer(rhi(), depthDestPass, depthDestColor,
			depthDest);
		ASSERT_TRUE(depthDestFb);

		ASSERT_TRUE(runPass(rhi(), depthSourcePass, depthSourceFb,
			{ { depthPipeline, topHalfAtDepth(kBothDraw, kBothDrawZ) } }));
		ASSERT_TRUE(runPass(rhi(), depthDestPass, depthDestFb, {}));

		const auto sourceDepthPixels = readTarget(rhi(), depthSource);
		ASSERT_EQ(sourceDepthPixels.size(), static_cast<size_t>(kSize) * kSize * 4u)
			<< "the depth blit source did not read back";
		for (int y = 0; y < kSize; ++y) {
			const int expected = y < kTopRows ? kBothDrawStored : kBothClearStored;
			for (int x = 0; x < kSize; ++x) {
				ASSERT_NEAR(depthValueAt(sourceDepthPixels, x, y), expected, kBothTolerance)
					<< "the depth blit source does not hold two depths at " << x << "," << y;
			}
		}

		runBlit(rhi(), depthSourceFb, depthDestFb, BlitMask::Depth, BlitFilter::Nearest);

		const auto destDepthPixels = readTarget(rhi(), depthDest);
		ASSERT_EQ(destDepthPixels.size(), static_cast<size_t>(kSize) * kSize * 4u)
			<< "the depth blit destination did not read back";
		const auto destColorPixels = readTarget(rhi(), depthDestColor);
		ASSERT_EQ(destColorPixels.size(), static_cast<size_t>(kSize) * kSize * 4u)
			<< "the depth blit destination's colour attachment did not read back";
		for (int y = 0; y < kSize; ++y) {
			for (int x = 0; x < kSize; ++x) {
				// Source and destination are the same format, so the copy itself owes an exact value;
				// the tolerance above is only for what the two backends' depth formats rounded to.
				EXPECT_EQ(depthValueAt(destDepthPixels, x, y), depthValueAt(sourceDepthPixels, x, y))
					<< "the depth blit did not reproduce the source depth at " << x << "," << y
					<< (depthValueAt(destDepthPixels, x, y) == kBlitDepthDstStored
						? " -- the destination is still holding its own clearDepth, so BlitMask::Depth "
						  "moved nothing"
						: "");
				EXPECT_EQ(pixelAt(destColorPixels, x, y), kBlitDepthDstClearExpected)
					<< "a BlitMask::Depth blit also wrote the destination's colour attachment at "
					<< x << "," << y;
			}
		}

		rhi().destroyFramebuffer(depthDestFb);
		rhi().destroyFramebuffer(depthSourceFb);
		rhi().destroyGraphicsPipeline(depthPipeline);
		rhi().destroyRenderPass(depthDestPass);
		rhi().destroyRenderPass(depthSourcePass);
		rhi().destroyTexture(depthDest);
		rhi().destroyTexture(depthDestColor);
		rhi().destroyTexture(depthSource);
		rhi().destroyTexture(depthSourceColor);

		rhi().destroyFramebuffer(linearHalfFb);
		rhi().destroyFramebuffer(nearestHalfFb);
		rhi().destroyFramebuffer(copyFb);
		rhi().destroyFramebuffer(sourceFb);
		rhi().destroyRenderPass(colorPass);
		rhi().destroyTexture(linearHalf);
		rhi().destroyTexture(nearestHalf);
		rhi().destroyTexture(copy);
		rhi().destroyTexture(source);
	}

	namespace {

		const Math::Vec4 kBackbufferClear{ unorm(9), unorm(19), unorm(29), unorm(39) };
		const Math::Vec4 kBackbufferDraw{ unorm(49), unorm(59), unorm(69), unorm(79) };
		const Rgba kBackbufferDrawExpected{ 49, 59, 69, 79 };
		const Math::Vec4 kAfterClear{ unorm(89), unorm(99), unorm(109), unorm(119) };
		const Math::Vec4 kAfterDraw{ unorm(129), unorm(139), unorm(149), unorm(159) };
		const Rgba kAfterClearExpected{ 89, 99, 109, 119 };
		const Rgba kAfterDrawExpected{ 129, 139, 149, 159 };
	}

	// getBackbufferFramebuffer is the one handle a caller does not own: the present path asks for it to
	// put the finished frame on the screen. A headless host -- the editor rendering into a texture it
	// hands to ImGui, and this whole suite -- never gets a screen, so what the call answers with then is
	// a contract in its own right, and the two backends do not answer the same way.
	//
	// OpenGL returns a *null* handle, unconditionally: the body is `return {}` and nothing else
	// (OpenGLRHI.cpp:1721-1724). Null is not an error value there -- it is how the backend spells "the
	// default framebuffer", and both call sites read it that way, binding FBO 0 in execBeginRenderPass
	// (OpenGLRHI.cpp:1404-1413) and resolving to GL framebuffer 0 sized from the window in execBlit
	// (OpenGLRHI.cpp:2714-2733). So on a hidden window the calls land on that window's real default
	// framebuffer.
	//
	// Vulkan returns a *valid* handle (VulkanRHIPipelines.cpp:1028-1030) to a framebuffer built in init
	// whether or not anything will ever be presented -- a resource with swapchainTarget set, no
	// VkFramebuffer and no attachments (VulkanRHIPipelines.cpp:278-291, VulkanRHI.cpp:281). Both call
	// sites test that flag instead of the handle: cmdBeginRenderPass returns without recording anything
	// (VulkanRHIPipelines.cpp:1317-1319) and cmdBlit diverts into the present path
	// (VulkanRHIPipelines.cpp:1572-1586).
	//
	// That difference is the finding, and it is not papered over below: a caller cannot ask "did I get a
	// backbuffer?" and get a portable answer, because `if (rhi.getBackbufferFramebuffer())` is false on
	// one backend and true on the other for the identical headless host. Nothing here asserts either
	// value.
	//
	// There is no draw inside the backbuffer pass, and that is deliberate rather than caution. Vulkan's
	// cmdBeginRenderPass rejects the handle *silently*, so the cmdBindPipeline and cmdDraw that follow
	// are recorded outside any render pass instance; the measured result is an access violation inside
	// the draw, not a validation message. Calling it here would only be testing whether the process
	// happens to survive. It is reported as a defect instead.
	//
	// What is left is what both backends do agree on, and it is what a caller actually needs: the handle
	// is stable, it is never one of the caller's own framebuffers, a pass and a blit aimed at it complete,
	// the blit's source is not disturbed, and -- the real assertion -- an ordinary offscreen pass
	// afterwards still lands exactly. The last one matters most on Vulkan, where the blit is not a copy
	// at all but a latch: it stores the source texture as the present source
	// (VulkanRHIPipelines.cpp:1582), so every later present in the test takes the present-blit path
	// rather than the clear path it took before.
	TEST_P(RhiBackendTest, TakingTheHeadlessBackbufferFramebufferLeavesTheDeviceUsable) {
		const auto backbuffer = rhi().getBackbufferFramebuffer();
		EXPECT_EQ(rhi().getBackbufferFramebuffer().id, backbuffer.id)
			<< "two calls in a row answered with different handles";

		const auto pass = makeClearPass(rhi(), kBackbufferClear);
		ASSERT_TRUE(pass);
		const auto pipeline = makePipeline(rhi(), pass);
		ASSERT_TRUE(pipeline);
		const auto source = makeSizedColorTarget(rhi(), kSize, kSize, nullptr);
		ASSERT_TRUE(source);
		const auto sourceFb = makeSizedColorFramebuffer(rhi(), pass, kSize, kSize, source);
		ASSERT_TRUE(sourceFb);
		EXPECT_NE(sourceFb.id, backbuffer.id)
			<< "the backbuffer is a handle the caller could have been given by createFramebuffer, so "
			   "destroying their own framebuffer would take the backbuffer with it";

		ASSERT_TRUE(runPass(rhi(), pass, sourceFb, { { pipeline, fullScreen(kBackbufferDraw) } }));

		runClearOnlyPass(rhi(), pass, backbuffer, nullptr, nullptr);
		runBlit(rhi(), sourceFb, backbuffer, BlitMask::Color, BlitFilter::Nearest);

		ASSERT_NO_FATAL_FAILURE(expectUniform(rhi(), source, kBackbufferDrawExpected,
			"the blit into the backbuffer disturbed its own source"));

		const auto afterTarget = makeTarget(rhi());
		ASSERT_TRUE(afterTarget);
		const auto afterPass = makeClearPass(rhi(), kAfterClear);
		ASSERT_TRUE(afterPass);
		const auto afterPipeline = makePipeline(rhi(), afterPass);
		ASSERT_TRUE(afterPipeline);
		const auto afterFb = makeFramebuffer(rhi(), afterPass, afterTarget);
		ASSERT_TRUE(afterFb);

		ASSERT_TRUE(runPass(rhi(), afterPass, afterFb, { { afterPipeline, topHalf(kAfterDraw) } }));
		{
			const auto pixels = readTarget(rhi(), afterTarget);
			ASSERT_FALSE(pixels.empty())
				<< "an ordinary offscreen target stopped reading back once the backbuffer had been used";
			for (int y = 0; y < kSize; ++y) {
				const Rgba expected = y < kTopRows ? kAfterDrawExpected : kAfterClearExpected;
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< (y < kTopRows
							? "an ordinary offscreen draw after the backbuffer was used did not land at "
							: "an ordinary offscreen clear after the backbuffer was used did not land at ")
						<< x << "," << y;
				}
			}
		}

		rhi().destroyFramebuffer(afterFb);
		rhi().destroyGraphicsPipeline(afterPipeline);
		rhi().destroyRenderPass(afterPass);
		rhi().destroyTexture(afterTarget);
		rhi().destroyFramebuffer(sourceFb);
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(source);
	}

	namespace {

		// Window depth is 0.5 * z + 0.5, and a driver may evaluate it either that way or as (z + 1) * 0.5.
		// Both clip z values are multiples of 2^-24 in [-1, -0.5], so z, z + 1, 0.5 * z and the final window
		// depth are all exact in float32 whichever order is used: A lands on 0.25 - 2^-25 and B on
		// 0.25 - 2^-24. They are distinct float32 values, and 2^-25 is also the finest spacing that stays
		// exact under both orders. Converted to 24-bit unorm with the round-to-nearest both APIs specify,
		// A is 4194303.25 and B is 4194302.75: the same integer, so a 24-bit buffer sees B as equal and
		// Less rejects it, while a float or a real 32-bit unorm buffer lets B through.
		constexpr float kPrecisionZA = -0.5f - 0x1p-24f;
		constexpr float kPrecisionZB = -0.5f - 0x1p-23f;
		const Math::Vec4 kPrecisionRed{ 1.f, 0.f, 0.f, 1.f };
		const Math::Vec4 kPrecisionGreen{ 0.f, 1.f, 0.f, 1.f };
		const Rgba kPrecisionRedExpected{ 255, 0, 0, 255 };
		const Rgba kPrecisionGreenExpected{ 0, 255, 0, 255 };

		TextureHandle makeDepthTargetOfFormat(RenderHardwareInterface& rhi, TextureFormat format) {
			TextureDesc desc;
			desc.width = kSize;
			desc.height = kSize;
			desc.depth = 1;
			desc.arrayLayers = 1;
			desc.dimension = TextureDimension::Tex2D;
			desc.format = format;
			desc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled
				| TextureUsage::TransferSrc;
			return rhi.createTexture(desc);
		}

		RenderPassHandle makeColorAndDepthPassOfFormat(RenderHardwareInterface& rhi, TextureFormat format) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = { 0.f, 0.f, 0.f, 1.f };
			desc.hasDepthStencil = true;
			desc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
			desc.depthAttachment.storeOp = AttachmentStoreOp::Store;
			desc.depthAttachment.format = format;
			desc.depthAttachment.clearDepth = 1.f;
			return rhi.createRenderPass(desc);
		}

		TrianglePush fullScreenAtDepth(const Math::Vec4& color, float z) {
			TrianglePush push = fullScreen(color);
			push.a.z = z;
			push.b.z = z;
			push.c.z = z;
			return push;
		}
	}

	// DEPTH_COMPONENT24 and DEPTH_COMPONENT32 are integer unorm depth on OpenGL but D32_SFLOAT on Vulkan,
	// so the same enumerator can resolve a depth difference on one backend and not the other. Readback
	// quantises depth to 16 bits and would hide it; the depth test does not. The expectation is the
	// Vulkan result for both formats, B in front, since the backends are meant to agree.
	TEST_P(RhiBackendTest, DepthFormatsResolveTheSameSubStepDepthDifferenceOnBothBackends) {
		for (const auto format : { TextureFormat::DEPTH_COMPONENT24, TextureFormat::DEPTH_COMPONENT32 }) {
			const char* const name = format == TextureFormat::DEPTH_COMPONENT24 ? "DEPTH_COMPONENT24" : "DEPTH_COMPONENT32";

			const auto color = makeTarget(rhi());
			ASSERT_TRUE(color);
			const auto depth = makeDepthTargetOfFormat(rhi(), format);
			ASSERT_TRUE(depth) << name;
			const auto pass = makeColorAndDepthPassOfFormat(rhi(), format);
			ASSERT_TRUE(pass) << name;
			const auto pipeline = makeDepthTestingPipeline(rhi(), pass);
			ASSERT_TRUE(pipeline) << name;
			const auto fb = makeColorAndDepthFramebuffer(rhi(), pass, color, depth);
			ASSERT_TRUE(fb) << name;

			ASSERT_TRUE(runPass(rhi(), pass, fb, {
				{ pipeline, fullScreenAtDepth(kPrecisionRed, kPrecisionZA) },
				{ pipeline, fullScreenAtDepth(kPrecisionGreen, kPrecisionZB) },
			}));

			const auto pixels = readTarget(rhi(), color);
			ASSERT_EQ(pixels.size(), static_cast<size_t>(kSize) * kSize * 4u) << name;
			const Rgba texel = pixelAt(pixels, kSize / 2, kSize / 2);
			EXPECT_EQ(texel, kPrecisionGreenExpected)
				<< name << ": got " << int(texel.r) << "," << int(texel.g) << "," << int(texel.b) << ","
				<< int(texel.a) << (texel == kPrecisionRedExpected
					? " (red: the nearer quad lost, the format did not resolve 2^-25)" : "");

			rhi().destroyFramebuffer(fb);
			rhi().destroyGraphicsPipeline(pipeline);
			rhi().destroyRenderPass(pass);
			rhi().destroyTexture(depth);
			rhi().destroyTexture(color);
		}
	}

	// The cascaded shadow map's shape: a DEPTH_COMPONENT32 array rendered one layer at a time through
	// a framebuffer that attaches that single layer (CascadedShadowPass, one framebuffer per cascade),
	// then read two ways. readbackTexture pulls each layer to the CPU; a sampler2DArray draw reads the
	// same layers on the GPU. Each layer holds a different depth, so a layer that kept its clear, a
	// draw that landed in the wrong layer and a readback that read the wrong layer all fail differently,
	// and comparing the two reads says whether a wrong answer is in the render or in the readback.
	TEST_P(RhiBackendTest, EachLayerOfADepthArrayKeepsItsOwnDepthThroughReadbackAndSampling) {
		constexpr uint32_t kLayers = 3;
		// Window depth 0.25, 0.5 and 0.75.
		constexpr float kLayerZ[kLayers] = { -0.5f, 0.f, 0.5f };
		constexpr int kLayerStored[kLayers] = { 16384, 32768, 49151 };
		constexpr int kLayerSampled[kLayers] = { 64, 128, 191 };

		TextureDesc depthDesc;
		depthDesc.width = kSize;
		depthDesc.height = kSize;
		depthDesc.depth = kLayers;
		depthDesc.arrayLayers = kLayers;
		depthDesc.dimension = TextureDimension::Array;
		depthDesc.format = TextureFormat::DEPTH_COMPONENT32;
		depthDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		const auto depth = rhi().createTexture(depthDesc);
		ASSERT_TRUE(depth);

		RenderPassDesc depthPassDesc;
		depthPassDesc.hasDepthStencil = true;
		depthPassDesc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
		depthPassDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
		depthPassDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
		depthPassDesc.depthAttachment.clearDepth = 1.f;
		const auto depthPass = rhi().createRenderPass(depthPassDesc);
		ASSERT_TRUE(depthPass);

		GraphicsPipelineDesc depthPipeDesc;
		depthPipeDesc.renderPass = depthPass;
		depthPipeDesc.topology = PrimitiveTopology::Triangles;
		depthPipeDesc.cullMode = CullMode::None;
		depthPipeDesc.frontFace = FrontFace::CounterClockWise;
		depthPipeDesc.fillMode = PolygonFillMode::Fill;
		depthPipeDesc.depthStencilState = { true, true, CompareOp::Less };
		depthPipeDesc.blend = { false };
		depthPipeDesc.pushConstantSize = sizeof(TrianglePush);
		depthPipeDesc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		depthPipeDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
		depthPipeDesc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_depth_only.fs");
		const auto depthPipeline = rhi().createGraphicsPipeline(depthPipeDesc);
		ASSERT_TRUE(depthPipeline);

		std::vector<FramebufferHandle> layerFbs;
		for (uint32_t layer = 0; layer < kLayers; ++layer) {
			FramebufferDesc fbDesc;
			fbDesc.width = kSize;
			fbDesc.height = kSize;
			fbDesc.layers = 1;
			fbDesc.renderPass = depthPass;
			FramebufferAttachmentDesc att;
			att.texture = depth;
			att.mipLevel = 0;
			att.layer = layer;
			fbDesc.depthStencilAttachment = att;
			layerFbs.push_back(rhi().createFramebuffer(fbDesc));
			ASSERT_TRUE(layerFbs.back()) << "layer " << layer;
			ASSERT_TRUE(runPass(rhi(), depthPass, layerFbs.back(),
				{ { depthPipeline, fullScreenAtDepth({ 1.f, 1.f, 1.f, 1.f }, kLayerZ[layer]) } }));
		}

		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Nearest;
		samplerDesc.magFilter = FilterMode::Nearest;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		samplerDesc.addressW = AddressMode::ClampToEdge;
		samplerDesc.compareOp = CompareOp::None;
		const auto sampler = rhi().createSampler(samplerDesc);
		ASSERT_TRUE(sampler);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);
		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		write.images.push_back({ depth, sampler });
		rhi().updateDescriptorSets({ write });

		const auto color = makeTarget(rhi());
		ASSERT_TRUE(color);
		RenderPassDesc samplePassDesc;
		samplePassDesc.colorAttachmentCount = 1;
		samplePassDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
		samplePassDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		samplePassDesc.colorAttachments[0].format = TextureFormat::RGBA8;
		samplePassDesc.colorAttachments[0].clearColor = { 0.f, 0.f, 0.f, 0.f };
		const auto samplePass = rhi().createRenderPass(samplePassDesc);
		ASSERT_TRUE(samplePass);

		GraphicsPipelineDesc samplePipeDesc;
		samplePipeDesc.renderPass = samplePass;
		samplePipeDesc.topology = PrimitiveTopology::Triangles;
		samplePipeDesc.cullMode = CullMode::None;
		samplePipeDesc.frontFace = FrontFace::CounterClockWise;
		samplePipeDesc.fillMode = PolygonFillMode::Fill;
		samplePipeDesc.depthStencilState = { false, false, CompareOp::Always };
		samplePipeDesc.blend = { false };
		samplePipeDesc.descriptorSetLayouts = { layout };
		samplePipeDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_fullscreen.vs");
		samplePipeDesc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_depth_array.fs");
		const auto samplePipeline = rhi().createGraphicsPipeline(samplePipeDesc);
		ASSERT_TRUE(samplePipeline);
		const auto sampleFb = makeFramebuffer(rhi(), samplePass, color);
		ASSERT_TRUE(sampleFb);

		rhi().beginFrame();
		const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
		RenderPassBeginInfo begin;
		begin.renderPass = samplePass;
		begin.framebuffer = sampleFb;
		begin.width = kSize;
		begin.height = kSize;
		rhi().cmdBeginRenderPass(cmd, begin);
		rhi().cmdBindPipeline(cmd, samplePipeline);
		rhi().cmdBindDescriptorSets(cmd, samplePipeline, 0, &set, 1);
		rhi().cmdDraw(cmd, 3, 1, 0, 0);
		rhi().cmdEndRenderPass(cmd);
		rhi().endCommandList(cmd);
		rhi().submit(cmd);
		rhi().present();
		rhi().waitIdle();

		const auto sampled = readTarget(rhi(), color);
		ASSERT_EQ(sampled.size(), static_cast<size_t>(kSize) * kSize * 4u) << "the sampled target did not read back";
		const Rgba texel = pixelAt(sampled, kSize / 2, kSize / 2);
		const int sampledLayer[kLayers] = { texel.r, texel.g, texel.b };

		for (uint32_t layer = 0; layer < kLayers; ++layer) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			ASSERT_TRUE(rhi().readbackTexture(depth, pixels, w, h, layer)) << "layer " << layer;
			ASSERT_EQ(pixels.size(), static_cast<size_t>(kSize) * kSize * 4u) << "layer " << layer;
			EXPECT_NEAR(depthValueAt(pixels, kSize / 2, kSize / 2), kLayerStored[layer], kBothTolerance)
				<< "readbackTexture of layer " << layer << " (sampled " << sampledLayer[layer] << ")";
			EXPECT_NEAR(sampledLayer[layer], kLayerSampled[layer], 1)
				<< "sampler2DArray layer " << layer << " (readback "
				<< depthValueAt(pixels, kSize / 2, kSize / 2) << ")";
		}

		rhi().destroyFramebuffer(sampleFb);
		rhi().destroyGraphicsPipeline(samplePipeline);
		rhi().destroyRenderPass(samplePass);
		rhi().destroyTexture(color);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroySampler(sampler);
		for (const auto fb : layerFbs) {
			rhi().destroyFramebuffer(fb);
		}
		rhi().destroyGraphicsPipeline(depthPipeline);
		rhi().destroyRenderPass(depthPass);
		rhi().destroyTexture(depth);
	}

	// The cascade caster pipeline's vertex layout carries Vertex3D's bone ids as an Int4 attribute,
	// and its shader reads them to decide whether to skin. Layer 0 is drawn through a float-only
	// layout, layer 1 through the same buffer with the Int4 attribute added; both must keep the
	// draw's depth. An integer attribute the backend never formatted reads as (0,0,0,1), which the
	// shader turns into a dropped draw, so the layer keeps its clear with no error reported.
	TEST_P(RhiBackendTest, ADepthArrayLayerKeepsTheDepthOfADrawWithAnIntegerVertexAttribute) {
		constexpr uint32_t kLayers = 2;
		constexpr int kStored = 32768;

		struct Vertex {
			float pos[2];
			float color[4];
			int ids[4];
		};
		const Vertex verts[3] = {
			{ { -1.f, -1.f }, { 1.f, 1.f, 1.f, 1.f }, { -1, -1, -1, -1 } },
			{ { 3.f, -1.f }, { 1.f, 1.f, 1.f, 1.f }, { -1, -1, -1, -1 } },
			{ { -1.f, 3.f }, { 1.f, 1.f, 1.f, 1.f }, { -1, -1, -1, -1 } },
		};
		BufferDesc vboDesc;
		vboDesc.size = sizeof(verts);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.debugName = "rhi_test_int_attribute_vertex_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts);
		ASSERT_TRUE(vbo);

		TextureDesc depthDesc;
		depthDesc.width = kSize;
		depthDesc.height = kSize;
		depthDesc.depth = kLayers;
		depthDesc.arrayLayers = kLayers;
		depthDesc.dimension = TextureDimension::Array;
		depthDesc.format = TextureFormat::DEPTH_COMPONENT32;
		depthDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		const auto depth = rhi().createTexture(depthDesc);
		ASSERT_TRUE(depth);

		RenderPassDesc passDesc;
		passDesc.hasDepthStencil = true;
		passDesc.depthAttachment.loadOp = AttachmentLoadOp::Clear;
		passDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
		passDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
		passDesc.depthAttachment.clearDepth = 1.f;
		const auto pass = rhi().createRenderPass(passDesc);
		ASSERT_TRUE(pass);

		GraphicsPipelineDesc pipeDesc;
		pipeDesc.renderPass = pass;
		pipeDesc.topology = PrimitiveTopology::Triangles;
		pipeDesc.cullMode = CullMode::None;
		pipeDesc.frontFace = FrontFace::CounterClockWise;
		pipeDesc.fillMode = PolygonFillMode::Fill;
		pipeDesc.depthStencilState = { true, true, CompareOp::Less };
		pipeDesc.blend = { false };
		pipeDesc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_depth_only.fs");
		pipeDesc.layout.bindings.push_back({ 0, sizeof(Vertex), VertexInputRate::PerVertex });
		pipeDesc.layout.attributes.push_back({ 0, 0, VertexFormat::Float2, offsetof(Vertex, pos) });

		GraphicsPipelineDesc floatDesc = pipeDesc;
		floatDesc.layout.attributes.push_back({ 1, 0, VertexFormat::Float4, offsetof(Vertex, color) });
		floatDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_vertex.vs");
		const auto floatPipeline = rhi().createGraphicsPipeline(floatDesc);
		ASSERT_TRUE(floatPipeline);

		GraphicsPipelineDesc intDesc = pipeDesc;
		intDesc.layout.attributes.push_back({ 1, 0, VertexFormat::Int4, offsetof(Vertex, ids) });
		intDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_int_attribute.vs");
		const auto intPipeline = rhi().createGraphicsPipeline(intDesc);
		ASSERT_TRUE(intPipeline);

		const PipelineHandle layerPipeline[kLayers] = { floatPipeline, intPipeline };
		const char* const layerName[kLayers] = { "float-only layout", "layout with an Int4 attribute" };
		std::vector<FramebufferHandle> layerFbs;
		for (uint32_t layer = 0; layer < kLayers; ++layer) {
			FramebufferDesc fbDesc;
			fbDesc.width = kSize;
			fbDesc.height = kSize;
			fbDesc.layers = 1;
			fbDesc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = depth;
			att.mipLevel = 0;
			att.layer = layer;
			fbDesc.depthStencilAttachment = att;
			layerFbs.push_back(rhi().createFramebuffer(fbDesc));
			ASSERT_TRUE(layerFbs.back()) << layerName[layer];

			rhi().beginFrame();
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = layerFbs.back();
			begin.width = kSize;
			begin.height = kSize;
			rhi().cmdBeginRenderPass(cmd, begin);
			rhi().cmdBindPipeline(cmd, layerPipeline[layer]);
			rhi().cmdBindVertexBuffer(cmd, 0, vbo, 0);
			rhi().cmdDraw(cmd, 3, 1, 0, 0);
			rhi().cmdEndRenderPass(cmd);
			rhi().endCommandList(cmd);
			rhi().submit(cmd);
			rhi().present();
			rhi().waitIdle();
		}

		for (uint32_t layer = 0; layer < kLayers; ++layer) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			ASSERT_TRUE(rhi().readbackTexture(depth, pixels, w, h, layer)) << layerName[layer];
			ASSERT_EQ(pixels.size(), static_cast<size_t>(kSize) * kSize * 4u) << layerName[layer];
			EXPECT_NEAR(depthValueAt(pixels, kSize / 2, kSize / 2), kStored, kBothTolerance)
				<< "layer " << layer << " drawn through the " << layerName[layer];
		}

		for (const auto fb : layerFbs) {
			rhi().destroyFramebuffer(fb);
		}
		rhi().destroyGraphicsPipeline(intPipeline);
		rhi().destroyGraphicsPipeline(floatPipeline);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(depth);
		rhi().destroyBuffer(vbo);
	}
}
