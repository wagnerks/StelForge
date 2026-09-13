#include "RhiTestFixture.h"

namespace SFE::Render::Tests {
	// The harness itself: if a clear does not survive a readback, nothing else in the suite means
	// anything.
	TEST_P(RhiBackendTest, ClearSurvivesReadback) {
		const auto tex = makeColorArray(rhi(), 1);
		ASSERT_TRUE(tex);
		const auto pass = makeClearPass(rhi(), { 1.f, 0.f, 0.f, 1.f });
		const auto fb = makeLayerFramebuffer(rhi(), pass, tex, UINT32_MAX);
		ASSERT_TRUE(fb);

		runClearPass(rhi(), pass, fb);

		Rgba pixel{};
		ASSERT_TRUE(readPixel(rhi(), tex, 0, pixel));
		EXPECT_EQ(pixel, (Rgba{ 255, 0, 0, 255 }));
	}

	// A framebuffer that pins one array layer must write that layer and leave the others alone.
	// This is the contract the cascaded shadow pass relies on once it renders a cascade per layer,
	// and the Vulkan backend used to ignore the pinned layer entirely: it attached the texture's
	// whole-array view, so every pass landed on layer 0 and the other layers kept whatever they
	// had, in whatever image layout they had.
	TEST_P(RhiBackendTest, RenderPassWritesOnlyThePinnedLayer) {
		const auto tex = makeColorArray(rhi(), 3);
		ASSERT_TRUE(tex);

		const auto blackPass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		for (uint32_t layer = 0; layer < 3; ++layer) {
			const auto fb = makeLayerFramebuffer(rhi(), blackPass, tex, layer);
			ASSERT_TRUE(fb);
			runClearPass(rhi(), blackPass, fb);
		}

		const auto greenPass = makeClearPass(rhi(), { 0.f, 1.f, 0.f, 1.f });
		const auto targetFb = makeLayerFramebuffer(rhi(), greenPass, tex, 1);
		ASSERT_TRUE(targetFb);
		runClearPass(rhi(), greenPass, targetFb);

		Rgba layer0{};
		Rgba layer1{};
		Rgba layer2{};
		ASSERT_TRUE(readPixel(rhi(), tex, 0, layer0));
		ASSERT_TRUE(readPixel(rhi(), tex, 1, layer1));
		ASSERT_TRUE(readPixel(rhi(), tex, 2, layer2));

		EXPECT_EQ(layer1, (Rgba{ 0, 255, 0, 255 })) << "the pinned layer was not written";
		EXPECT_EQ(layer0, (Rgba{ 0, 0, 0, 255 })) << "layer 0 was written by a pass aimed at layer 1";
		EXPECT_EQ(layer2, (Rgba{ 0, 0, 0, 255 })) << "layer 2 was written by a pass aimed at layer 1";
	}

	INSTANTIATE_TEST_SUITE_P(Backends, RhiBackendTest,
		::testing::Values(GraphicsBackend::OpenGL, GraphicsBackend::Vulkan),
		RhiBackendTest::backendName);
}
