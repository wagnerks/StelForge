#include "RhiTestFixture.h"

// The conventions a pass has to guess at today: which way clip-space Y points, which winding counts
// as front, and what a depth compare means. Each one is asserted against a concrete expectation and
// checked on both backends, so a backend that disagrees fails by name instead of turning up weeks
// later as a shadow that lands in the wrong place.
namespace SFE::Render::Tests {
	namespace {

		struct TrianglePush {
			Math::Vec4 a{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 b{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 c{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f };
		};

		constexpr int kSize = 8;

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

		PipelineHandle makeTrianglePipeline(RenderHardwareInterface& rhi, RenderPassHandle pass,
			CullMode cull, FrontFace front) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = cull;
			desc.frontFace = front;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.pushConstantSize = sizeof(TrianglePush);
			desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		// Clears to black, draws the triangle, hands back the image top row first.
		std::vector<uint8_t> drawTriangle(RenderHardwareInterface& rhi, TextureHandle target,
			PipelineHandle pipeline, RenderPassHandle pass, const TrianglePush& push) {
			FramebufferDesc fbDesc;
			fbDesc.width = kSize;
			fbDesc.height = kSize;
			fbDesc.layers = 1;
			fbDesc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = target;
			att.layer = UINT32_MAX;
			fbDesc.colorAttachments = { att };
			const auto fb = rhi.createFramebuffer(fbDesc);
			if (!fb || !pipeline) {
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
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
			rhi.cmdDraw(cmd, 3, 1, 0, 0);
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

		// Covers the whole target, wound counter-clockwise in a Y-up clip space.
		TrianglePush fullScreenCcw() {
			TrianglePush push;
			push.a = { -1.f, -1.f, 0.f, 1.f };
			push.b = { 3.f, -1.f, 0.f, 1.f };
			push.c = { -1.f, 3.f, 0.f, 1.f };
			push.color = { 0.f, 1.f, 0.f, 1.f };
			return push;
		}
	}

	// A triangle wound counter-clockwise must survive back-face culling and disappear under
	// front-face culling, and it must do so identically on both backends. The cascaded shadow pass
	// carried an "if Vulkan" here for exactly this reason, and it was compensating for something the
	// RHI already handled, which left the backends storing opposite faces.
	TEST_P(RhiBackendTest, CounterClockwiseIsFrontFacingOnBothBackends) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);

		const auto keepPipeline = makeTrianglePipeline(rhi(), pass, CullMode::Back, FrontFace::CounterClockWise);
		ASSERT_TRUE(keepPipeline) << "pipeline creation failed";
		const auto kept = drawTriangle(rhi(), target, keepPipeline, pass, fullScreenCcw());
		ASSERT_FALSE(kept.empty());
		EXPECT_EQ(pixelAt(kept, kSize / 2, kSize / 2), (Rgba{ 0, 255, 0, 255 }))
			<< "a counter-clockwise triangle was culled by back-face culling";

		const auto cullPipeline = makeTrianglePipeline(rhi(), pass, CullMode::Front, FrontFace::CounterClockWise);
		ASSERT_TRUE(cullPipeline);
		const auto culled = drawTriangle(rhi(), target, cullPipeline, pass, fullScreenCcw());
		ASSERT_FALSE(culled.empty());
		EXPECT_EQ(pixelAt(culled, kSize / 2, kSize / 2), (Rgba{ 0, 0, 0, 255 }))
			<< "front-face culling did not remove a counter-clockwise triangle";
	}

	// Clip space Y points up: a triangle occupying the positive half must land in the upper half of
	// the image. Readback normalises both backends to top row first, so the expectation is the same
	// number on both, and a backend that rasterises upside down fails here rather than in a shadow
	// map that no one is looking at.
	TEST_P(RhiBackendTest, ClipSpacePositiveYLandsInTheTopHalf) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		const auto pipeline = makeTrianglePipeline(rhi(), pass, CullMode::None, FrontFace::CounterClockWise);
		ASSERT_TRUE(pipeline);

		TrianglePush push;
		push.a = { -1.f, 0.f, 0.f, 1.f };
		push.b = { 3.f, 0.f, 0.f, 1.f };
		push.c = { -1.f, 4.f, 0.f, 1.f };
		push.color = { 1.f, 0.f, 0.f, 1.f };

		const auto pixels = drawTriangle(rhi(), target, pipeline, pass, push);
		ASSERT_FALSE(pixels.empty());
		EXPECT_EQ(pixelAt(pixels, kSize / 2, 1), (Rgba{ 255, 0, 0, 255 })) << "top of the image is empty";
		EXPECT_EQ(pixelAt(pixels, kSize / 2, kSize - 2), (Rgba{ 0, 0, 0, 255 })) << "bottom of the image was painted";
	}

	// What a depth buffer actually holds. The shadow map is nothing but this, sampled later with a
	// comparison sampler, so if the two backends store a different number for the same clip-space z
	// every shadow comparison is off by that difference. The light matrices are built for OpenGL's
	// [-1, 1] clip range and Vulkan is configured to match through depth clip control, so z = 0 must
	// land in the middle of the range on both.
	TEST_P(RhiBackendTest, ClipSpaceZeroDepthStoresMidRangeOnBothBackends) {
		TextureDesc depthDesc;
		depthDesc.width = kSize;
		depthDesc.height = kSize;
		depthDesc.depth = 1;
		depthDesc.arrayLayers = 1;
		depthDesc.dimension = TextureDimension::Tex2D;
		depthDesc.format = TextureFormat::DEPTH_COMPONENT32;
		depthDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		const auto depthTex = rhi().createTexture(depthDesc);
		ASSERT_TRUE(depthTex);

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
		pipeDesc.depthStencilState = { true, true, CompareOp::LessEqual };
		pipeDesc.blend = { false };
		pipeDesc.pushConstantSize = sizeof(TrianglePush);
		pipeDesc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		pipeDesc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
		pipeDesc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_depth_only.fs");
		const auto pipeline = rhi().createGraphicsPipeline(pipeDesc);
		ASSERT_TRUE(pipeline);

		FramebufferDesc fbDesc;
		fbDesc.width = kSize;
		fbDesc.height = kSize;
		fbDesc.layers = 1;
		fbDesc.renderPass = pass;
		FramebufferAttachmentDesc att;
		att.texture = depthTex;
		att.layer = UINT32_MAX;
		fbDesc.depthStencilAttachment = att;
		const auto fb = rhi().createFramebuffer(fbDesc);
		ASSERT_TRUE(fb);

		TrianglePush push = fullScreenCcw();
		push.a.z = 0.f;
		push.b.z = 0.f;
		push.c.z = 0.f;

		rhi().beginFrame();
		const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
		RenderPassBeginInfo begin;
		begin.renderPass = pass;
		begin.framebuffer = fb;
		begin.width = kSize;
		begin.height = kSize;
		rhi().cmdBeginRenderPass(cmd, begin);
		rhi().cmdBindPipeline(cmd, pipeline);
		rhi().cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
		rhi().cmdDraw(cmd, 3, 1, 0, 0);
		rhi().cmdEndRenderPass(cmd);
		rhi().endCommandList(cmd);
		rhi().submit(cmd);
		rhi().present();
		rhi().waitIdle();

		std::vector<uint8_t> pixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(depthTex, pixels, w, h));
		// Depth comes back as 16 bits across R and G, R being the high byte.
		const auto centre = pixelAt(pixels, kSize / 2, kSize / 2);
		const int stored = (static_cast<int>(centre.r) << 8) | static_cast<int>(centre.g);
		EXPECT_NEAR(stored, 32768, 512) << "clip z = 0 did not land mid range; stored " << stored;
	}
}
