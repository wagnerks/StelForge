#include "RhiTestFixture.h"

#include "assetsModule/shaderModule/ShaderBase.h"

namespace SFE::Render::Tests {

	TEST_P(RhiBackendTest, BackendReportsTheBackendTheFixtureBuilt) {
		const auto first = rhi().backend();
		EXPECT_EQ(first, GetParam());
		EXPECT_EQ(rhi().backend(), first);
	}

	// One desc serves both backends only because each reads a different field: OpenGL compiles
	// `source` and ignores `name` (OpenGLRHI.cpp:2343), Vulkan ignores `source` and loads
	// shaders/vulkan/<basename of name>.spv (VulkanRHIPipelines.cpp:854-863). So the desc carries
	// both the path and the preprocessed GLSL.
	TEST_P(RhiBackendTest, ShaderModuleFromDescDrawsAndDestroysAfterPipelineCreation) {
		auto& r = rhi();
		auto makeDesc = [](ShaderStage stage, const char* path) {
			ShaderModuleDesc desc;
			desc.stage = stage;
			desc.name = path;
			desc.source = ShaderModule::ShaderBase::loadShaderCode(path);
			return desc;
		};
		const auto vs = r.createShaderModule(makeDesc(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs"));
		const auto fs = r.createShaderModule(makeDesc(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs"));
		ASSERT_TRUE(vs);
		ASSERT_TRUE(fs);

		const auto target = makeColorArray(r, 1);
		const auto pass = makeClearPass(r, { 0.f, 0.f, 0.f, 0.f });
		const auto fb = makeLayerFramebuffer(r, pass, target, 0);

		struct TrianglePush {
			Math::Vec4 a{ -1.f, -1.f, 0.f, 1.f };
			Math::Vec4 b{ 3.f, -1.f, 0.f, 1.f };
			Math::Vec4 c{ -1.f, 3.f, 0.f, 1.f };
			Math::Vec4 color{ 64.f / 255.f, 128.f / 255.f, 192.f / 255.f, 1.f };
		};

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
		desc.shaders.vertex = vs;
		desc.shaders.fragment = fs;
		const auto pipeline = r.createGraphicsPipeline(desc);
		ASSERT_TRUE(pipeline);

		// Destroying the modules once the pipeline exists is legal on both: GL keeps a linked program
		// after glDeleteShader, and Vulkan never needs a VkShaderModule after pipeline creation. The
		// draw below proves the pipeline does not reach back into them.
		r.destroyShaderModule(vs);
		r.destroyShaderModule(fs);

		r.beginFrame();
		const auto cmd = r.beginCommandList(CommandListType::Graphics);
		RenderPassBeginInfo begin;
		begin.renderPass = pass;
		begin.framebuffer = fb;
		begin.width = 4;
		begin.height = 4;
		r.cmdBeginRenderPass(cmd, begin);
		r.cmdBindPipeline(cmd, pipeline);
		TrianglePush push;
		r.cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
		r.cmdDraw(cmd, 3, 1, 0, 0);
		r.cmdEndRenderPass(cmd);
		r.endCommandList(cmd);
		r.submit(cmd);
		r.present();
		r.waitIdle();

		Rgba px;
		ASSERT_TRUE(readPixel(r, target, 0, px));
		EXPECT_EQ(px, (Rgba{ 64, 128, 192, 255 }));

		r.destroyShaderModule(ShaderModuleHandle{});
		r.destroyShaderModule(vs);
		r.destroyShaderModule(fs);

		r.destroyGraphicsPipeline(pipeline);
		r.destroyFramebuffer(fb);
		r.destroyRenderPass(pass);
		r.destroyTexture(target);
	}

	TEST_P(RhiBackendTest, PresentSourceTextureReportsTheTextureHandedToSetPresentBlitSource) {
		auto& r = rhi();
		EXPECT_FALSE(r.presentSourceTexture());

		const auto pass = makeClearPass(r, { 1.f, 0.f, 0.f, 1.f });
		const auto greenPass = makeClearPass(r, { 0.f, 1.f, 0.f, 1.f });
		const auto a = makeColorArray(r, 1);
		const auto b = makeColorArray(r, 1);
		ASSERT_TRUE(a);
		ASSERT_TRUE(b);
		const auto fbA = makeLayerFramebuffer(r, pass, a, 0);
		const auto fbB = makeLayerFramebuffer(r, greenPass, b, 0);
		runClearPass(r, pass, fbA);
		runClearPass(r, greenPass, fbB);

		std::vector<uint8_t> pixels;
		int w = 0;
		int h = 0;

		// Each runClearPass also presents, so every state below goes through present() with a source set.
		r.setPresentBlitSource(a, SamplerHandle{});
		EXPECT_EQ(r.presentSourceTexture().id, a.id);
		runClearPass(r, pass, fbA);
		ASSERT_TRUE(r.readbackPresentSource(pixels, w, h));
		EXPECT_EQ((Rgba{ pixels[0], pixels[1], pixels[2], pixels[3] }), (Rgba{ 255, 0, 0, 255 }));

		r.setPresentBlitSource(b, SamplerHandle{});
		EXPECT_EQ(r.presentSourceTexture().id, b.id);
		runClearPass(r, greenPass, fbB);
		ASSERT_TRUE(r.readbackPresentSource(pixels, w, h));
		EXPECT_EQ((Rgba{ pixels[0], pixels[1], pixels[2], pixels[3] }), (Rgba{ 0, 255, 0, 255 }));

		// Neither destroyTexture clears the stored source, so the dead handle is still reported.
		r.destroyFramebuffer(fbB);
		r.destroyTexture(b);
		EXPECT_EQ(r.presentSourceTexture().id, b.id);
		EXPECT_FALSE(r.readbackPresentSource(pixels, w, h));
		runClearPass(r, pass, fbA);

		r.setPresentBlitSource(TextureHandle{}, SamplerHandle{});
		EXPECT_FALSE(r.presentSourceTexture());
		runClearPass(r, pass, fbA);

		r.destroyFramebuffer(fbA);
		r.destroyTexture(a);
		r.destroyRenderPass(greenPass);
		r.destroyRenderPass(pass);
	}

	namespace {
		struct SizedTarget {
			TextureHandle tex;
			FramebufferHandle fb;
		};

		SizedTarget makeSizedTarget(RenderHardwareInterface& rhi, RenderPassHandle pass, uint32_t size) {
			TextureDesc tex;
			tex.width = size;
			tex.height = size;
			tex.format = TextureFormat::RGBA8;
			tex.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
			SizedTarget out;
			out.tex = rhi.createTexture(tex);
			FramebufferDesc fb;
			fb.width = size;
			fb.height = size;
			fb.layers = 1;
			fb.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = out.tex;
			fb.colorAttachments = { att };
			out.fb = rhi.createFramebuffer(fb);
			return out;
		}

		void clearSized(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb, uint32_t size) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = size;
			begin.height = size;
			rhi.cmdBeginRenderPass(cmd, begin);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

	}

	TEST_P(RhiBackendTest, PublishDisplayOnAHeadlessHostCapturesTheFramebufferAndLeavesTheDeviceUsable) {
		auto& r = rhi();
		std::vector<uint8_t> rgba;
		int w = -1;
		int h = -1;

		EXPECT_EQ(r.editorViewNativeId(), 0u);
		EXPECT_FALSE(r.capturePublishedFrame(rgba, w, h));
		EXPECT_EQ(w, -1);
		EXPECT_EQ(h, -1);
		EXPECT_TRUE(rgba.empty());

		// Nothing is live to publish yet: no framebuffer, and no present blit source to fall back to.
		r.publishDisplay({});
		EXPECT_EQ(r.editorViewNativeId(), 0u);
		EXPECT_FALSE(r.capturePublishedFrame(rgba, w, h));

		const auto red = makeClearPass(r, { 1.f, 0.f, 0.f, 1.f });
		const auto green = makeClearPass(r, { 0.f, 1.f, 0.f, 1.f });
		const auto small = makeSizedTarget(r, red, 4);
		const auto large = makeSizedTarget(r, green, 8);
		ASSERT_TRUE(small.tex);
		ASSERT_TRUE(small.fb);
		ASSERT_TRUE(large.tex);
		ASSERT_TRUE(large.fb);

		auto expectUniform = [&](int size, Rgba color) {
			ASSERT_TRUE(r.capturePublishedFrame(rgba, w, h));
			EXPECT_EQ(w, size);
			EXPECT_EQ(h, size);
			ASSERT_EQ(rgba.size(), static_cast<size_t>(size) * size * 4);
			for (size_t i = 0; i < rgba.size(); i += 4) {
				ASSERT_EQ((Rgba{ rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3] }), color) << "byte " << i;
			}
		};

		clearSized(r, red, small.fb, 4);
		r.publishDisplay(small.fb);
		EXPECT_NE(r.editorViewNativeId(), 0u);
		r.waitIdle();
		expectUniform(4, { 255, 0, 0, 255 });

		// A different size makes both backends drop and rebuild their display textures.
		clearSized(r, green, large.fb, 8);
		r.publishDisplay(large.fb);
		EXPECT_NE(r.editorViewNativeId(), 0u);
		r.waitIdle();
		expectUniform(8, { 0, 255, 0, 255 });

		// Back to the first size, so the rebuild runs over textures that were themselves rebuilt.
		clearSized(r, red, small.fb, 4);
		r.publishDisplay(small.fb);
		EXPECT_NE(r.editorViewNativeId(), 0u);
		r.waitIdle();
		expectUniform(4, { 255, 0, 0, 255 });

		// With something published, a null framebuffer still leaves the last frame capturable.
		r.publishDisplay({});
		EXPECT_NE(r.editorViewNativeId(), 0u);
		r.waitIdle();
		expectUniform(4, { 255, 0, 0, 255 });

		const auto blue = makeClearPass(r, { 0.f, 0.f, 1.f, 1.f });
		const auto after = makeColorArray(r, 1);
		ASSERT_TRUE(after);
		const auto afterFb = makeLayerFramebuffer(r, blue, after, 0);
		runClearPass(r, blue, afterFb);
		Rgba px;
		ASSERT_TRUE(readPixel(r, after, 0, px));
		EXPECT_EQ(px, (Rgba{ 0, 0, 255, 255 }));

		r.setPresentBlitSource(TextureHandle{}, SamplerHandle{});
		r.destroyFramebuffer(afterFb);
		r.destroyTexture(after);
		r.destroyFramebuffer(large.fb);
		r.destroyTexture(large.tex);
		r.destroyFramebuffer(small.fb);
		r.destroyTexture(small.tex);
		r.destroyRenderPass(blue);
		r.destroyRenderPass(green);
		r.destroyRenderPass(red);
	}

	// Vulkan shares one VkShaderModule per SPIR-V name and reference-counts it; OpenGL compiles a
	// fresh shader per create. Either way each create must behave as its own module.
	TEST_P(RhiBackendTest, ShaderModulesFromTheSameDescAreIndependentAndRecreatable) {
		auto& r = rhi();
		auto makeDesc = [](ShaderStage stage, const char* path) {
			ShaderModuleDesc desc;
			desc.stage = stage;
			desc.name = path;
			desc.source = ShaderModule::ShaderBase::loadShaderCode(path);
			return desc;
		};
		const auto vsDesc = makeDesc(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
		const auto fsDesc = makeDesc(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs");

		const auto target = makeColorArray(r, 1);
		const auto pass = makeClearPass(r, { 0.f, 0.f, 0.f, 0.f });
		const auto fb = makeLayerFramebuffer(r, pass, target, 0);

		struct TrianglePush {
			Math::Vec4 a{ -1.f, -1.f, 0.f, 1.f };
			Math::Vec4 b{ 3.f, -1.f, 0.f, 1.f };
			Math::Vec4 c{ -1.f, 3.f, 0.f, 1.f };
			Math::Vec4 color{ 64.f / 255.f, 128.f / 255.f, 192.f / 255.f, 1.f };
		};

		auto drawWith = [&](ShaderModuleHandle vs, ShaderModuleHandle fs) {
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
			desc.shaders.vertex = vs;
			desc.shaders.fragment = fs;
			const auto pipeline = r.createGraphicsPipeline(desc);
			ASSERT_TRUE(pipeline);

			r.beginFrame();
			const auto cmd = r.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = 4;
			begin.height = 4;
			r.cmdBeginRenderPass(cmd, begin);
			r.cmdBindPipeline(cmd, pipeline);
			TrianglePush push;
			r.cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &push, sizeof(push));
			r.cmdDraw(cmd, 3, 1, 0, 0);
			r.cmdEndRenderPass(cmd);
			r.endCommandList(cmd);
			r.submit(cmd);
			r.present();
			r.waitIdle();

			Rgba px;
			const bool read = readPixel(r, target, 0, px);
			r.destroyGraphicsPipeline(pipeline);
			ASSERT_TRUE(read);
			EXPECT_EQ(px, (Rgba{ 64, 128, 192, 255 }));
		};

		const auto v1 = r.createShaderModule(vsDesc);
		const auto f1 = r.createShaderModule(fsDesc);
		const auto v2 = r.createShaderModule(vsDesc);
		const auto f2 = r.createShaderModule(fsDesc);
		ASSERT_TRUE(v1);
		ASSERT_TRUE(f1);
		ASSERT_TRUE(v2);
		ASSERT_TRUE(f2);

		r.destroyShaderModule(v1);
		r.destroyShaderModule(f1);
		drawWith(v2, f2);

		r.destroyShaderModule(v2);
		r.destroyShaderModule(f2);
		const auto v3 = r.createShaderModule(vsDesc);
		const auto f3 = r.createShaderModule(fsDesc);
		ASSERT_TRUE(v3);
		ASSERT_TRUE(f3);
		drawWith(v3, f3);

		r.destroyShaderModule(v3);
		r.destroyShaderModule(f3);
		r.destroyFramebuffer(fb);
		r.destroyRenderPass(pass);
		r.destroyTexture(target);
	}
}
