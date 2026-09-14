#include <gtest/gtest.h>

#include <cstring>
#include <set>

#include "rhi/RhiFactory.h"

// The None backend draws nothing, so the pixel tests do not run on it. What the engine relies on is that it
// behaves like a device that never fails: valid, distinct handles and a frame that goes through.
namespace SFE::Render::Tests {
	TEST(NullRhiTest, HandsOutDistinctValidHandles) {
		auto rhi = createRhi(GraphicsBackend::None);
		ASSERT_TRUE(rhi);
		EXPECT_EQ(rhi->backend(), GraphicsBackend::None);
		rhi->init();

		std::set<uint32_t> ids;
		const auto add = [&](uint32_t id) {
			EXPECT_NE(id, UINT32_MAX);
			EXPECT_TRUE(ids.insert(id).second) << "id " << id << " handed out twice";
		};
		add(rhi->createTexture(TextureDesc{}).id);
		add(rhi->createBuffer(BufferDesc{ .size = 64 }).id);
		add(rhi->createSampler(SamplerDesc{}).id);
		add(rhi->createRenderPass(RenderPassDesc{}).id);
		add(rhi->createFramebuffer(FramebufferDesc{}).id);
		const auto layout = rhi->createDescriptorSetLayout(DescriptorSetLayoutDesc{});
		add(layout.id);
		add(rhi->allocateDescriptorSet(layout).id);
		add(rhi->createComputePipeline(ComputePipelineDesc{}).id);
		add(rhi->createGraphicsPipeline(GraphicsPipelineDesc{}).id);
		EXPECT_TRUE(rhi->getBackbufferFramebuffer());

		rhi->cleanup();
	}

	TEST(NullRhiTest, RunsAFrameAndReadsNothingBack) {
		auto rhi = createRhi(GraphicsBackend::None);
		rhi->init();

		const auto buffer = rhi->createBuffer(BufferDesc{ .size = 32 });
		auto* mapped = static_cast<uint8_t*>(rhi->mapBuffer(buffer, MapMode::Write));
		ASSERT_NE(mapped, nullptr);
		std::memset(mapped, 0xAB, 32);
		rhi->unmapBuffer(buffer);

		bool ran = false;
		rhi->runOnWorkerThread([&] { ran = true; });
		EXPECT_TRUE(ran);

		rhi->beginFrame();
		const auto cmd = rhi->beginCommandList(CommandListType::Graphics);
		EXPECT_TRUE(cmd);
		rhi->cmdDraw(cmd, 3);
		rhi->endCommandList(cmd);
		rhi->submit(cmd);
		rhi->present();

		std::vector<uint8_t> rgba;
		int width = 0;
		int height = 0;
		EXPECT_FALSE(rhi->readbackTexture(rhi->createTexture(TextureDesc{}), rgba, width, height));

		rhi->destroyBuffer(buffer);
		rhi->cleanup();
	}
}
