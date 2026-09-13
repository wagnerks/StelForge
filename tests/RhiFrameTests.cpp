#include "RhiTestFixture.h"

// Frames and synchronisation. framesInFlight() is the number the whole engine sizes its per-frame
// arrays by -- Batcher, GeometryPass and RenderSystem all ring-buffer their GPU resources modulo it
// -- so the number is only worth anything if that many frames can genuinely be outstanding at once
// without the backend reusing one frame's internals under another frame's feet. That is what this
// file pins first.
namespace SFE::Render::Tests {
	namespace {

		constexpr int kSize = 8;

		struct TrianglePush {
			Math::Vec4 a{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 b{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 c{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f };
		};

		constexpr float unorm(int byteValue) { return static_cast<float>(byteValue) / 255.f; }

		// One colour per frame, no two sharing a channel value with each other or with the clear, so a
		// frame that landed in the wrong target reads back as another frame's colour by name rather than
		// as a plausible number. The clear is the value a frame that was dropped outright leaves behind.
		const Math::Vec4 kFrameColors[] = {
			{ unorm(16), unorm(32), unorm(48), unorm(64) },
			{ unorm(80), unorm(96), unorm(112), unorm(128) },
			{ unorm(144), unorm(160), unorm(176), unorm(192) },
			{ unorm(208), unorm(224), unorm(240), unorm(252) },
		};

		const Rgba kFrameBytes[] = {
			{ 16, 32, 48, 64 },
			{ 80, 96, 112, 128 },
			{ 144, 160, 176, 192 },
			{ 208, 224, 240, 252 },
		};

		constexpr uint32_t kMaxFramesCovered = static_cast<uint32_t>(std::size(kFrameColors));

		const Math::Vec4 kNeverDrawn{ unorm(5), unorm(10), unorm(15), unorm(20) };

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

		// Deliberately no waitIdle: the caller drives framesInFlight() of these back to back, and a wait
		// between them would retire each frame before the next began, which is the one arrangement that
		// cannot tell a genuine per-frame resource from a single shared one.
		void submitFrame(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, const Math::Vec4& color) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			rhi.cmdBindPipeline(cmd, pipeline);
			TrianglePush push;
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { 3.f, -1.f, 0.f, 1.f };
			push.c = { -1.f, 3.f, 0.f, 1.f };
			push.color = color;
			rhi.cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
			rhi.cmdDraw(cmd, 3, 1, 0, 0);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
		}

		Rgba pixelAt(const std::vector<uint8_t>& pixels, int x, int y) {
			const size_t offset = (static_cast<size_t>(y) * kSize + static_cast<size_t>(x)) * 4u;
			if (offset + 3 >= pixels.size()) {
				return {};
			}
			return { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
		}
	}

	// framesInFlight() consecutive frames, each writing its own colour into its own target, with no
	// wait anywhere between them, and every colour still there at the end. Three distinct failures are
	// in range and each names itself: a frame whose recording was clobbered by the next one leaves its
	// target at the clear colour, a frame that executed against the wrong per-frame state leaves
	// another frame's colour in the target, and a backend that cannot actually keep that many frames
	// outstanding faults or hangs instead of reaching the readback. The number itself is only asserted
	// to be at least one, because the two backends return different values for it and a per-backend
	// expectation here would hide that rather than pin it: OpenGL never overrides framesInFlight() and
	// takes the interface default of 1 (RenderHardwareInterface.h:496), Vulkan returns 2
	// (VulkanRHI.h:129, from kMaxFramesInFlight at VulkanRHI.h:348). At 1 the test still says
	// something: one frame, submitted and presented, survives with its own colour and nothing else's.
	TEST_P(RhiBackendTest, FramesInFlightConsecutiveFramesEachKeepTheirOwnWrite) {
		const uint32_t frames = rhi().framesInFlight();
		ASSERT_GE(frames, 1u) << "framesInFlight() must name at least the frame the caller is inside";
		ASSERT_LE(frames, kMaxFramesCovered)
			<< "framesInFlight() returned " << frames << ", more than the " << kMaxFramesCovered
			<< " distinct colours this test carries";

		const auto pass = makeClearPass(rhi(), kNeverDrawn);
		ASSERT_TRUE(pass);
		const auto pipeline = makePipeline(rhi(), pass);
		ASSERT_TRUE(pipeline);

		std::vector<TextureHandle> targets(frames);
		std::vector<FramebufferHandle> framebuffers(frames);
		for (uint32_t i = 0; i < frames; ++i) {
			targets[i] = makeTarget(rhi());
			ASSERT_TRUE(targets[i]) << "target for frame " << i;
			framebuffers[i] = makeFramebuffer(rhi(), pass, targets[i]);
			ASSERT_TRUE(framebuffers[i]) << "framebuffer for frame " << i;
		}

		for (uint32_t i = 0; i < frames; ++i) {
			submitFrame(rhi(), pass, framebuffers[i], pipeline, kFrameColors[i]);
		}

		rhi().waitIdle();

		EXPECT_EQ(rhi().framesInFlight(), frames)
			<< "framesInFlight() changed while frames were outstanding, so nothing sized by it is safe";

		for (uint32_t i = 0; i < frames; ++i) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			ASSERT_TRUE(rhi().readbackTexture(targets[i], pixels, w, h)) << "readback of frame " << i;
			ASSERT_EQ(w, kSize);
			ASSERT_EQ(h, kSize);
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), kFrameBytes[i])
						<< "frame " << i << " of " << frames << " did not survive at " << x << "," << y;
				}
			}
		}

		for (uint32_t i = 0; i < frames; ++i) {
			rhi().destroyFramebuffer(framebuffers[i]);
			rhi().destroyTexture(targets[i]);
		}
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyRenderPass(pass);
	}

	// Read before each frame (beginFrame does not move the index, so this is the value the frame runs
	// under) and after its present. 2 * framesInFlight() + 1 frames cover the wrap at least twice.
	TEST_P(RhiBackendTest, FrameInFlightAdvancesByOnePerFrameAndWraps) {
		const uint32_t frames = rhi().framesInFlight();
		ASSERT_GE(frames, 1u);

		const auto pass = makeClearPass(rhi(), kNeverDrawn);
		ASSERT_TRUE(pass);
		const auto pipeline = makePipeline(rhi(), pass);
		ASSERT_TRUE(pipeline);
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto fb = makeFramebuffer(rhi(), pass, target);
		ASSERT_TRUE(fb);

		const uint32_t total = 2 * frames + 1;
		std::vector<uint32_t> before(total);
		std::vector<uint32_t> after(total);
		for (uint32_t i = 0; i < total; ++i) {
			before[i] = rhi().frameInFlight();
			submitFrame(rhi(), pass, fb, pipeline, kFrameColors[0]);
			after[i] = rhi().frameInFlight();
		}

		for (uint32_t i = 0; i < total; ++i) {
			EXPECT_EQ(before[i], i % frames) << "index frame " << i << " ran under, of " << frames;
			EXPECT_EQ(after[i], (i + 1) % frames) << "index after frame " << i << " presented, of " << frames;
		}

		rhi().waitIdle();
		rhi().destroyFramebuffer(fb);
		rhi().destroyTexture(target);
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyRenderPass(pass);
	}

	// Three lists submitted one after another in one frame write the same target with nothing between
	// them but a Load: A over everything, B over columns 0-3, C over columns 0-1. Only column bands are
	// used so the readback row flip cannot matter. In order they leave C|C|B|B|A..., and swapping the
	// last two must leave B|B|B|B|A..., which shows the submission order was honoured rather than
	// happening to match.
	TEST_P(RhiBackendTest, ListsSubmittedInOneFrameExecuteInSubmissionOrder) {
		const auto clearPass = makeClearPass(rhi(), kNeverDrawn);
		ASSERT_TRUE(clearPass);
		RenderPassDesc loadDesc;
		loadDesc.colorAttachmentCount = 1;
		loadDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		loadDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		loadDesc.colorAttachments[0].format = TextureFormat::RGBA8;
		const auto loadPass = rhi().createRenderPass(loadDesc);
		ASSERT_TRUE(loadPass);
		const auto clearPipeline = makePipeline(rhi(), clearPass);
		ASSERT_TRUE(clearPipeline);
		const auto loadPipeline = makePipeline(rhi(), loadPass);
		ASSERT_TRUE(loadPipeline);
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto clearFb = makeFramebuffer(rhi(), clearPass, target);
		ASSERT_TRUE(clearFb);
		const auto loadFb = makeFramebuffer(rhi(), loadPass, target);
		ASSERT_TRUE(loadFb);

		// Two triangles spanning NDC x in [-1, right] over the full height.
		const auto submitBand = [&](bool clear, float right, const Math::Vec4& color) {
			const auto pass = clear ? clearPass : loadPass;
			const auto pipeline = clear ? clearPipeline : loadPipeline;
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = clear ? clearFb : loadFb;
			begin.width = kSize;
			begin.height = kSize;
			rhi().cmdBeginRenderPass(cmd, begin);
			rhi().cmdBindPipeline(cmd, pipeline);
			TrianglePush push;
			push.color = color;
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { right, -1.f, 0.f, 1.f };
			push.c = { right, 1.f, 0.f, 1.f };
			rhi().cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
			rhi().cmdDraw(cmd, 3, 1, 0, 0);
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { right, 1.f, 0.f, 1.f };
			push.c = { -1.f, 1.f, 0.f, 1.f };
			rhi().cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
			rhi().cmdDraw(cmd, 3, 1, 0, 0);
			rhi().cmdEndRenderPass(cmd);
			rhi().endCommandList(cmd);
			rhi().submit(cmd);
		};

		const auto runFrame = [&](bool cLast, std::vector<uint8_t>& pixels) {
			rhi().beginFrame();
			submitBand(true, 1.f, kFrameColors[0]);
			if (cLast) {
				submitBand(false, 0.f, kFrameColors[1]);
				submitBand(false, -0.5f, kFrameColors[2]);
			}
			else {
				submitBand(false, -0.5f, kFrameColors[2]);
				submitBand(false, 0.f, kFrameColors[1]);
			}
			rhi().present();
			rhi().waitIdle();
			int w = 0;
			int h = 0;
			return rhi().readbackTexture(target, pixels, w, h) && w == kSize && h == kSize;
		};

		const auto expectBands = [&](const std::vector<uint8_t>& pixels, int cColumns, const char* order) {
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					const Rgba& expected = x < cColumns ? kFrameBytes[2] : x < kSize / 2 ? kFrameBytes[1] : kFrameBytes[0];
					EXPECT_EQ(pixelAt(pixels, x, y), expected) << "order " << order << " at " << x << "," << y;
				}
			}
		};

		std::vector<uint8_t> inOrder;
		ASSERT_TRUE(runFrame(true, inOrder)) << "readback after A,B,C";
		expectBands(inOrder, kSize / 4, "A,B,C");

		std::vector<uint8_t> swapped;
		ASSERT_TRUE(runFrame(false, swapped)) << "readback after A,C,B";
		expectBands(swapped, 0, "A,C,B");

		rhi().destroyFramebuffer(loadFb);
		rhi().destroyFramebuffer(clearFb);
		rhi().destroyTexture(target);
		rhi().destroyGraphicsPipeline(loadPipeline);
		rhi().destroyGraphicsPipeline(clearPipeline);
		rhi().destroyRenderPass(loadPass);
		rhi().destroyRenderPass(clearPass);
	}

	// cmdUpdateBuffer writes at the call, not when the list runs. One list: a draw into target A, then
	// cmdUpdateBuffer, then a draw into target B, both reading the same buffer. Both targets must show
	// the new value, A included. Run for a uniform and a storage buffer: Vulkan keeps the first
	// host-mapped and the second device-local, and the two take different write paths.
	TEST_P(RhiBackendTest, CmdUpdateBufferWritesAtTheCallSoEveryDrawInTheListSeesIt) {
		const Math::Vec4 kBefore = kFrameColors[0];
		const Math::Vec4 kAfter = kFrameColors[1];

		const auto pass = makeClearPass(rhi(), kNeverDrawn);
		ASSERT_TRUE(pass);

		struct Case {
			const char* name;
			BufferUsage usage;
			DescriptorType type;
			const char* fragment;
		};
		const Case cases[] = {
			{ "uniform", BufferUsage::Uniform, DescriptorType::UniformBuffer, "shaders/rhi_test_uniform.fs" },
			{ "storage", BufferUsage::Storage, DescriptorType::StorageBuffer, "shaders/rhi_test_storage_color.fs" },
		};
		for (const Case& c : cases) {
			SCOPED_TRACE(c.name);

			DescriptorSetLayoutDesc layoutDesc;
			layoutDesc.bindings.push_back({ 0, c.type, ShaderStage::Fragment });
			const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
			ASSERT_TRUE(layout);
			const auto set = rhi().allocateDescriptorSet(layout);
			ASSERT_TRUE(set);

			BufferDesc bufferDesc;
			bufferDesc.size = sizeof(Math::Vec4);
			bufferDesc.usage = c.usage;
			bufferDesc.cpuAccess = CpuAccess::Write;
			bufferDesc.debugName = "rhi_test_cmd_update";
			const auto buffer = rhi().createBuffer(bufferDesc, &kBefore);
			ASSERT_TRUE(buffer);

			WriteDescriptor write{};
			write.set = set;
			write.binding = 0;
			write.buffers.push_back({ buffer, 0, 0 });
			rhi().updateDescriptorSets({ write });

			GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.renderPass = pass;
			pipelineDesc.topology = PrimitiveTopology::Triangles;
			pipelineDesc.cullMode = CullMode::None;
			pipelineDesc.frontFace = FrontFace::CounterClockWise;
			pipelineDesc.fillMode = PolygonFillMode::Fill;
			pipelineDesc.depthStencilState = { false, false, CompareOp::Always };
			pipelineDesc.blend = { false };
			pipelineDesc.descriptorSetLayouts = { layout };
			pipelineDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_fullscreen.vs");
			pipelineDesc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, c.fragment);
			const auto pipeline = rhi().createGraphicsPipeline(pipelineDesc);
			ASSERT_TRUE(pipeline);

			const auto targetA = makeTarget(rhi());
			ASSERT_TRUE(targetA);
			const auto targetB = makeTarget(rhi());
			ASSERT_TRUE(targetB);
			const auto fbA = makeFramebuffer(rhi(), pass, targetA);
			ASSERT_TRUE(fbA);
			const auto fbB = makeFramebuffer(rhi(), pass, targetB);
			ASSERT_TRUE(fbB);

			rhi().beginFrame();
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
			const auto recordDraw = [&](FramebufferHandle fb) {
				RenderPassBeginInfo begin;
				begin.renderPass = pass;
				begin.framebuffer = fb;
				begin.width = kSize;
				begin.height = kSize;
				rhi().cmdBeginRenderPass(cmd, begin);
				rhi().cmdBindPipeline(cmd, pipeline);
				rhi().cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
				rhi().cmdDraw(cmd, 3, 1, 0, 0);
				rhi().cmdEndRenderPass(cmd);
			};
			recordDraw(fbA);
			rhi().cmdUpdateBuffer(cmd, buffer, 0, sizeof(kAfter), &kAfter);
			recordDraw(fbB);
			rhi().endCommandList(cmd);
			rhi().submit(cmd);
			rhi().present();
			rhi().waitIdle();

			const auto expectAll = [&](TextureHandle target, const char* what) {
				std::vector<uint8_t> pixels;
				int w = 0;
				int h = 0;
				ASSERT_TRUE(rhi().readbackTexture(target, pixels, w, h)) << what;
				ASSERT_EQ(w, kSize);
				ASSERT_EQ(h, kSize);
				for (int y = 0; y < kSize; ++y) {
					for (int x = 0; x < kSize; ++x) {
						EXPECT_EQ(pixelAt(pixels, x, y), kFrameBytes[1]) << what << " at " << x << "," << y;
					}
				}
			};
			expectAll(targetA, "draw recorded before cmdUpdateBuffer");
			expectAll(targetB, "draw recorded after cmdUpdateBuffer");

			rhi().destroyFramebuffer(fbB);
			rhi().destroyFramebuffer(fbA);
			rhi().destroyTexture(targetB);
			rhi().destroyTexture(targetA);
			rhi().destroyGraphicsPipeline(pipeline);
			rhi().destroyBuffer(buffer);
			rhi().freeDescriptorSet(set);
			rhi().destroyDescriptorSetLayout(layout);
		}
		rhi().destroyRenderPass(pass);
	}

	// One list: pass 1 draws a colour into T, pass 2 samples T into U, with and without a
	// cmdPipelineBarrier between them. Neither backend moves an image with it: Vulkan records a bare
	// VkMemoryBarrier with no image or buffer barriers, and OpenGL calls glMemoryBarrier, which only
	// has something to wait for after image load/store or SSBO writes. So U must hold T's colour both
	// times. Each run writes a different colour so a T left over from the previous run cannot pass.
	// U clears to a fourth colour, so "pass 2 never drew" and "pass 2 sampled an unwritten T" (which
	// reads kNeverDrawn) are each named.
	TEST_P(RhiBackendTest, CmdPipelineBarrierWriteThenSampleInOneListSeesTheWriteWithAndWithout) {
		const auto passT = makeClearPass(rhi(), kNeverDrawn);
		ASSERT_TRUE(passT);
		const auto passU = makeClearPass(rhi(), kFrameColors[3]);
		ASSERT_TRUE(passU);
		const auto writePipeline = makePipeline(rhi(), passT);
		ASSERT_TRUE(writePipeline);

		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Nearest;
		samplerDesc.magFilter = FilterMode::Nearest;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		const auto sampler = rhi().createSampler(samplerDesc);
		ASSERT_TRUE(sampler);

		const auto targetT = makeTarget(rhi());
		ASSERT_TRUE(targetT);
		const auto targetU = makeTarget(rhi());
		ASSERT_TRUE(targetU);
		const auto fbT = makeFramebuffer(rhi(), passT, targetT);
		ASSERT_TRUE(fbT);
		const auto fbU = makeFramebuffer(rhi(), passU, targetU);
		ASSERT_TRUE(fbU);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);
		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		write.images.push_back({ targetT, sampler });
		rhi().updateDescriptorSets({ write });

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.renderPass = passU;
		pipelineDesc.topology = PrimitiveTopology::Triangles;
		pipelineDesc.cullMode = CullMode::None;
		pipelineDesc.frontFace = FrontFace::CounterClockWise;
		pipelineDesc.fillMode = PolygonFillMode::Fill;
		pipelineDesc.depthStencilState = { false, false, CompareOp::Always };
		pipelineDesc.blend = { false };
		pipelineDesc.descriptorSetLayouts = { layout };
		pipelineDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_fullscreen.vs");
		pipelineDesc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_sampler.fs");
		const auto samplePipeline = rhi().createGraphicsPipeline(pipelineDesc);
		ASSERT_TRUE(samplePipeline);

		struct Case {
			const char* name;
			bool barrier;
			uint32_t color;
		};
		const Case cases[] = {
			{ "without barrier", false, 0 },
			{ "with barrier", true, 1 },
		};
		for (const Case& c : cases) {
			SCOPED_TRACE(c.name);

			rhi().beginFrame();
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);

			RenderPassBeginInfo beginT;
			beginT.renderPass = passT;
			beginT.framebuffer = fbT;
			beginT.width = kSize;
			beginT.height = kSize;
			rhi().cmdBeginRenderPass(cmd, beginT);
			rhi().cmdBindPipeline(cmd, writePipeline);
			TrianglePush push;
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { 3.f, -1.f, 0.f, 1.f };
			push.c = { -1.f, 3.f, 0.f, 1.f };
			push.color = kFrameColors[c.color];
			rhi().cmdPushConstants(cmd, writePipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
			rhi().cmdDraw(cmd, 3, 1, 0, 0);
			rhi().cmdEndRenderPass(cmd);

			if (c.barrier) {
				rhi().cmdPipelineBarrier(cmd, {
					PipelineStage::ColorAttachmentOutput,
					PipelineStage::FragmentShader,
					AccessMask::ColorAttachmentWrite,
					AccessMask::ShaderRead,
				});
			}

			RenderPassBeginInfo beginU;
			beginU.renderPass = passU;
			beginU.framebuffer = fbU;
			beginU.width = kSize;
			beginU.height = kSize;
			rhi().cmdBeginRenderPass(cmd, beginU);
			rhi().cmdBindPipeline(cmd, samplePipeline);
			rhi().cmdBindDescriptorSets(cmd, samplePipeline, 0, &set, 1);
			rhi().cmdDraw(cmd, 3, 1, 0, 0);
			rhi().cmdEndRenderPass(cmd);

			rhi().endCommandList(cmd);
			rhi().submit(cmd);
			rhi().present();
			rhi().waitIdle();

			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			ASSERT_TRUE(rhi().readbackTexture(targetU, pixels, w, h));
			ASSERT_EQ(w, kSize);
			ASSERT_EQ(h, kSize);
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), kFrameBytes[c.color]) << "U at " << x << "," << y;
				}
			}
		}

		rhi().destroyGraphicsPipeline(samplePipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyFramebuffer(fbU);
		rhi().destroyFramebuffer(fbT);
		rhi().destroyTexture(targetU);
		rhi().destroyTexture(targetT);
		rhi().destroySampler(sampler);
		rhi().destroyGraphicsPipeline(writePipeline);
		rhi().destroyRenderPass(passU);
		rhi().destroyRenderPass(passT);
	}
}

namespace SFE::Render::Tests {
	// Both backends start at 0.0 and only ever overwrite it with a measured query result, so before
	// the first frame the value is exactly 0. The result lags: OpenGL reads the previous submit's
	// GL_TIME_ELAPSED query inside the next submit (OpenGLRHI.cpp:954-962), Vulkan reads a slot's
	// timestamps in the beginFrame that reuses that slot, framesInFlight() frames later
	// (VulkanRHIResources.cpp:105, VulkanRHI.cpp:876). waitIdle after each frame makes the OpenGL
	// result available by the next submit instead of whenever the driver gets there. Many full-screen
	// draws into a large target keep the measured span well clear of timer resolution.
	TEST_P(RhiBackendTest, LastGpuMsIsZeroBeforeAnyFrameAndPositiveAfterRealWork) {
		constexpr uint32_t kBigSize = 1024;
		constexpr int kDrawsPerFrame = 8;
		constexpr int kMaxFrames = 10;

		EXPECT_EQ(rhi().lastGpuMs(), 0.0) << "value before any frame";

		const auto pass = makeClearPass(rhi(), kNeverDrawn);
		ASSERT_TRUE(pass);
		const auto pipeline = makePipeline(rhi(), pass);
		ASSERT_TRUE(pipeline);

		TextureDesc texDesc;
		texDesc.width = kBigSize;
		texDesc.height = kBigSize;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		const auto target = rhi().createTexture(texDesc);
		ASSERT_TRUE(target);

		FramebufferDesc fbDesc;
		fbDesc.width = kBigSize;
		fbDesc.height = kBigSize;
		fbDesc.layers = 1;
		fbDesc.renderPass = pass;
		FramebufferAttachmentDesc att;
		att.texture = target;
		att.layer = UINT32_MAX;
		fbDesc.colorAttachments = { att };
		const auto fb = rhi().createFramebuffer(fbDesc);
		ASSERT_TRUE(fb);

		int positiveAfter = -1;
		for (int frame = 0; frame < kMaxFrames && positiveAfter < 0; ++frame) {
			rhi().beginFrame();
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kBigSize;
			begin.height = kBigSize;
			rhi().cmdBeginRenderPass(cmd, begin);
			rhi().cmdBindPipeline(cmd, pipeline);
			TrianglePush push;
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { 3.f, -1.f, 0.f, 1.f };
			push.c = { -1.f, 3.f, 0.f, 1.f };
			for (int d = 0; d < kDrawsPerFrame; ++d) {
				push.color = kFrameColors[d % kMaxFramesCovered];
				rhi().cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
				rhi().cmdDraw(cmd, 3, 1, 0, 0);
			}
			rhi().cmdEndRenderPass(cmd);
			rhi().endCommandList(cmd);
			rhi().submit(cmd);
			rhi().present();
			rhi().waitIdle();

			const double ms = rhi().lastGpuMs();
			EXPECT_GE(ms, 0.0) << "after frame " << frame;
			if (ms > 0.0) {
				positiveAfter = frame;
			}
		}
		EXPECT_GE(positiveAfter, 0) << "lastGpuMs() never rose above 0 within " << kMaxFrames << " frames";

		rhi().destroyFramebuffer(fb);
		rhi().destroyTexture(target);
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyRenderPass(pass);
	}
}
