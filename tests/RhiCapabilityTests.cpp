#include "RhiTestFixture.h"


namespace SFE::Render::Tests {

	TEST_P(RhiBackendTest, BackendReportsTheBackendTheFixtureBuilt) {
		const auto first = rhi().backend();
		EXPECT_EQ(first, GetParam());
		EXPECT_EQ(rhi().backend(), first);
	}

	// A module built straight from a desc: both backends compile its include-expanded GLSL `source`,
	// read here through the host exactly as loadShader reads it.
	TEST_P(RhiBackendTest, ShaderModuleFromDescDrawsAndDestroysAfterPipelineCreation) {
		auto& r = rhi();
		auto makeDesc = [&r](ShaderStage stage, const char* path) {
			ShaderModuleDesc desc;
			desc.stage = stage;
			desc.name = path;
			desc.source = r.host()->loadShaderSource(path);
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
		ASSERT_TRUE(r.readbackTexture(r.presentSourceTexture(), pixels, w, h));
		EXPECT_EQ((Rgba{ pixels[0], pixels[1], pixels[2], pixels[3] }), (Rgba{ 255, 0, 0, 255 }));

		r.setPresentBlitSource(b, SamplerHandle{});
		EXPECT_EQ(r.presentSourceTexture().id, b.id);
		runClearPass(r, greenPass, fbB);
		ASSERT_TRUE(r.readbackTexture(r.presentSourceTexture(), pixels, w, h));
		EXPECT_EQ((Rgba{ pixels[0], pixels[1], pixels[2], pixels[3] }), (Rgba{ 0, 255, 0, 255 }));

		// Neither destroyTexture clears the stored source, so the dead handle is still reported.
		r.destroyFramebuffer(fbB);
		r.destroyTexture(b);
		EXPECT_EQ(r.presentSourceTexture().id, b.id);
		EXPECT_FALSE(r.readbackTexture(r.presentSourceTexture(), pixels, w, h));
		runClearPass(r, pass, fbA);

		r.setPresentBlitSource(TextureHandle{}, SamplerHandle{});
		EXPECT_FALSE(r.presentSourceTexture());
		runClearPass(r, pass, fbA);

		r.destroyFramebuffer(fbA);
		r.destroyTexture(a);
		r.destroyRenderPass(greenPass);
		r.destroyRenderPass(pass);
	}

	// Vulkan shares one VkShaderModule per SPIR-V name and reference-counts it; OpenGL compiles a
	// fresh shader per create. Either way each create must behave as its own module.
	TEST_P(RhiBackendTest, ShaderModulesFromTheSameDescAreIndependentAndRecreatable) {
		auto& r = rhi();
		auto makeDesc = [&r](ShaderStage stage, const char* path) {
			ShaderModuleDesc desc;
			desc.stage = stage;
			desc.name = path;
			desc.source = r.host()->loadShaderSource(path);
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
