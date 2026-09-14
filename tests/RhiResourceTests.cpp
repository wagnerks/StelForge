#include "RhiTestFixture.h"

#include <cstring>
#include <thread>

// Resource lifecycle: create, use, destroy, create again. No pipeline and no shaders, so this is
// the part of the interface that can be pinned down before the drawing half has anything to stand
// on. Every expectation is written once and checked on both backends.
namespace SFE::Render::Tests {
	namespace {

		BufferDesc uniformBufferDesc(size_t size, CpuAccess access) {
			BufferDesc desc;
			desc.size = size;
			desc.usage = BufferUsage::Uniform;
			desc.cpuAccess = access;
			return desc;
		}
	}

	TEST_P(RhiBackendTest, TextureHandleSurvivesDestroyAndRecreate) {
		const auto first = makeColorArray(rhi(), 1);
		ASSERT_TRUE(first);
		rhi().destroyTexture(first);

		// A destroyed slot must be handed out again. Vulkan once reset the slot before recycling it,
		// which made recycling a no-op: every texture and buffer id leaked and the pools only grew.
		const auto second = makeColorArray(rhi(), 1);
		EXPECT_TRUE(second);
		EXPECT_EQ(second.id, first.id);
		rhi().destroyTexture(second);

		BufferDesc bufferDesc;
		bufferDesc.size = 16;
		bufferDesc.usage = BufferUsage::Uniform;
		bufferDesc.cpuAccess = CpuAccess::Write;
		const auto firstBuffer = rhi().createBuffer(bufferDesc);
		ASSERT_TRUE(firstBuffer);
		rhi().destroyBuffer(firstBuffer);
		const auto secondBuffer = rhi().createBuffer(bufferDesc);
		EXPECT_TRUE(secondBuffer);
		EXPECT_EQ(secondBuffer.id, firstBuffer.id);
		rhi().destroyBuffer(secondBuffer);
	}

	TEST_P(RhiBackendTest, DestroyingATextureTwiceIsHarmless) {
		const auto tex = makeColorArray(rhi(), 1);
		ASSERT_TRUE(tex);
		rhi().destroyTexture(tex);
		// Shutdown paths double-free by accident often enough that this must not be a crash.
		rhi().destroyTexture(tex);
		SUCCEED();
	}

	// A loader thread creates a texture through runOnWorkerThread; the main thread then reads it. On
	// OpenGL that is the shared worker context, and the texture must be complete when the call returns.
	TEST_P(RhiBackendTest, TextureCreatedOnAWorkerThreadIsUsableOnTheMainThread) {
		const std::vector<uint8_t> source = {
			255,   0,   0, 255,    0, 255,   0, 255,
			  0,   0, 255, 255,  255, 255, 255, 255,
		};
		TextureHandle tex;
		std::thread loader([&] {
			rhi().runOnWorkerThread([&] {
				TextureDesc desc;
				desc.width = 2;
				desc.height = 2;
				desc.format = TextureFormat::RGBA8;
				desc.usage = TextureUsage::Sampled | TextureUsage::TransferSrc;
				tex = rhi().createTexture(desc, source.data());
			});
		});
		loader.join();
		ASSERT_TRUE(tex);

		std::vector<uint8_t> pixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(tex, pixels, w, h));
		ASSERT_EQ(pixels.size(), source.size());
		// Readback rows are top-down, the upload's first row is the bottom one.
		std::vector<uint8_t> flipped(source.size());
		const size_t rowBytes = 2u * 4u;
		std::memcpy(flipped.data(), source.data() + rowBytes, rowBytes);
		std::memcpy(flipped.data() + rowBytes, source.data(), rowBytes);
		EXPECT_EQ(pixels, flipped);

		rhi().destroyTexture(tex);
	}

	TEST_P(RhiBackendTest, TextureInitialDataReadsBackUnchanged) {
		// Four pixels, four different colours, so a transposed or flipped upload cannot pass by
		// accident the way a uniform fill would.
		const std::vector<uint8_t> source = {
			255,   0,   0, 255,    0, 255,   0, 255,
			  0,   0, 255, 255,  255, 255, 255, 255,
		};
		TextureDesc desc;
		desc.width = 2;
		desc.height = 2;
		desc.depth = 1;
		desc.arrayLayers = 1;
		desc.dimension = TextureDimension::Tex2D;
		desc.format = TextureFormat::RGBA8;
		desc.usage = TextureUsage::Sampled | TextureUsage::TransferSrc | TextureUsage::TransferDst;
		const auto tex = rhi().createTexture(desc, source.data());
		ASSERT_TRUE(tex);

		std::vector<uint8_t> pixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(tex, pixels, w, h));
		EXPECT_EQ(w, 2);
		EXPECT_EQ(h, 2);
		ASSERT_EQ(pixels.size(), source.size());

		// The two halves of the interface disagree about row order, and they disagree the same way
		// on both backends: an upload treats the first row as the bottom one, a readback hands rows
		// back top down. So the round trip comes back vertically flipped. That is a wart worth
		// straightening out, but it is the behaviour every existing capture and pass dump is built
		// on, so pin it here rather than let it drift unnoticed.
		std::vector<uint8_t> flipped(source.size());
		const size_t rowBytes = 2u * 4u;
		std::memcpy(flipped.data(), source.data() + rowBytes, rowBytes);
		std::memcpy(flipped.data() + rowBytes, source.data(), rowBytes);
		EXPECT_EQ(pixels, flipped);

		rhi().destroyTexture(tex);
	}

	TEST_P(RhiBackendTest, BufferInitialDataAndUpdateAreVisibleThroughMap) {
		const uint32_t initial[4] = { 1u, 2u, 3u, 4u };
		const auto buffer = rhi().createBuffer(uniformBufferDesc(sizeof(initial), CpuAccess::ReadWrite), initial);
		ASSERT_TRUE(buffer);

		if (auto* mapped = static_cast<const uint32_t*>(rhi().mapBuffer(buffer, MapMode::Read))) {
			EXPECT_EQ(mapped[0], 1u);
			EXPECT_EQ(mapped[3], 4u);
			rhi().unmapBuffer(buffer);
		}
		else {
			GTEST_SKIP() << "backend does not expose a readable mapping for uniform buffers";
		}

		const uint32_t replacement[4] = { 9u, 8u, 7u, 6u };
		rhi().updateBuffer(buffer, 0, sizeof(replacement), replacement);

		auto* mapped = static_cast<const uint32_t*>(rhi().mapBuffer(buffer, MapMode::Read));
		ASSERT_NE(mapped, nullptr);
		EXPECT_EQ(mapped[0], 9u);
		EXPECT_EQ(mapped[3], 6u);
		rhi().unmapBuffer(buffer);

		rhi().destroyBuffer(buffer);
	}

	TEST_P(RhiBackendTest, SamplerCreateAndDestroy) {
		SamplerDesc desc;
		desc.minFilter = FilterMode::Linear;
		desc.magFilter = FilterMode::Linear;
		desc.addressU = AddressMode::ClampToEdge;
		desc.addressV = AddressMode::ClampToEdge;
		const auto sampler = rhi().createSampler(desc);
		EXPECT_TRUE(sampler);
		rhi().destroySampler(sampler);
	}

	TEST_P(RhiBackendTest, RenderPassAndFramebufferOutliveEachOtherSafely) {
		const auto tex = makeColorArray(rhi(), 1);
		ASSERT_TRUE(tex);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);
		const auto fb = makeLayerFramebuffer(rhi(), pass, tex, UINT32_MAX);
		ASSERT_TRUE(fb);

		// Teardown order is the interesting part: a framebuffer holds a reference to its pass on
		// Vulkan, and passes are destroyed before framebuffers in more than one place in the engine.
		rhi().destroyRenderPass(pass);
		rhi().destroyFramebuffer(fb);
		rhi().destroyTexture(tex);
		SUCCEED();
	}

	TEST_P(RhiBackendTest, DescriptorSetLifecycle) {
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		EXPECT_TRUE(set);

		const uint32_t contents[4] = { 0u, 0u, 0u, 0u };
		const auto buffer = rhi().createBuffer(uniformBufferDesc(sizeof(contents), CpuAccess::Write), contents);
		ASSERT_TRUE(buffer);

		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		write.buffers.push_back({ buffer });
		rhi().updateDescriptorSets({ write });

		rhi().destroyBuffer(buffer);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		SUCCEED();
	}

	TEST_P(RhiBackendTest, FramesInFlightIsSane) {
		// Anything that sizes per-frame arrays trusts these two, and a backend reporting zero
		// frames or an index past the count would corrupt every one of them.
		EXPECT_GE(rhi().framesInFlight(), 1u);
		EXPECT_LT(rhi().frameInFlight(), rhi().framesInFlight());
	}
}
