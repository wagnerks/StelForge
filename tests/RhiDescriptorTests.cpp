#include "RhiTestFixture.h"

// Shader resources: the path from a descriptor set to a value a shader can actually read. The
// lifecycle test in RhiResourceTests already creates a layout, allocates a set, writes it and frees
// it, but nothing there ever binds a set or reads one in a shader, so "updateDescriptorSets
// succeeded" has so far meant nothing more than "the call returned". These tests close that gap by
// drawing with the set bound and reading the pixels back, which is the only evidence that the write
// reached the GPU on both backends.
namespace SFE::Render::Tests {
	namespace {

		constexpr int kSize = 4;

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

		RenderPassHandle makePass(RenderHardwareInterface& rhi, const Math::Vec4& clear) {
			RenderPassDesc desc;
			desc.colorAttachmentCount = 1;
			desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			desc.colorAttachments[0].format = TextureFormat::RGBA8;
			desc.colorAttachments[0].clearColor = clear;
			return rhi.createRenderPass(desc);
		}

		// A pipeline whose only input is its descriptor sets: rhi_test_fullscreen.vs builds its three
		// corners from the vertex index, so there is no vertex buffer and no push block to be confused
		// with the set under test. Nothing here is fixed-function state worth varying — culling off and
		// depth inert, the values the rest of the suite has already pinned — so the parameters are the
		// fragment shader and the set layouts, which is what the descriptor cases differ in.
		PipelineHandle makeDescriptorPipeline(RenderHardwareInterface& rhi, RenderPassHandle pass,
			const std::string& fragmentShader, const std::vector<DescriptorSetLayoutHandle>& layouts) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.descriptorSetLayouts = layouts;
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_fullscreen.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, fragmentShader);
			return rhi.createGraphicsPipeline(desc);
		}

		// One full-screen draw into a freshly cleared target, with `setCount` sets bound at `firstSet`.
		// Passing setCount == 0 records no cmdBindDescriptorSets at all, which is how a test proves the
		// draw really depends on the bind rather than on some leftover binding.
		std::vector<uint8_t> drawWithSets(RenderHardwareInterface& rhi, TextureHandle target,
			RenderPassHandle pass, PipelineHandle pipeline, const DescriptorSetHandle* sets,
			uint32_t setCount, uint32_t firstSet = 0, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0) {
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
			rhi.cmdBindPipeline(cmd, pipeline);
			if (setCount > 0) {
				// After the pipeline: Vulkan resolves the pipeline layout out of the handle passed here,
				// and OpenGL's own bind paths already require that order.
				rhi.cmdBindDescriptorSets(cmd, pipeline, firstSet, sets, setCount, dynamicOffsets, dynamicOffsetCount);
			}
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
			rhi.destroyFramebuffer(fb);
			return pixels;
		}

		Rgba pixelAt(const std::vector<uint8_t>& pixels, int x, int y) {
			const size_t offset = (static_cast<size_t>(y) * kSize + static_cast<size_t>(x)) * 4u;
			if (offset + 3 >= pixels.size()) {
				return {};
			}
			return { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
		}

		// Every channel a whole 1/255 step, so the float the shader writes lands on one exact byte and
		// the two backends can be held to the same number with no tolerance at all. A value like 0.3
		// would leave the rounding rule of each backend's conversion inside the expectation.
		constexpr float k255(int byteValue) { return static_cast<float>(byteValue) / 255.f; }

		BufferHandle makeUniformBuffer(RenderHardwareInterface& rhi, const Math::Vec4& value) {
			BufferDesc desc;
			desc.size = sizeof(Math::Vec4);
			desc.usage = BufferUsage::Uniform;
			desc.cpuAccess = CpuAccess::Write;
			desc.debugName = "rhi_test_descriptor_uniform";
			return rhi.createBuffer(desc, &value);
		}
	}

	// The whole descriptor path in one draw: a colour put in a uniform buffer, described by a layout,
	// written into a set, bound before the draw, and read back out of the attachment. Every step is
	// load bearing, so a failure names the first one that broke rather than "the frame looks wrong".
	TEST_P(RhiBackendTest, DescriptorUniformBufferReachesFragmentShader) {
		// Distinct in all four channels and distinct from the clear in all four, so a stale binding, a
		// dropped write or a channel swap each produce a different wrong answer rather than the same one.
		const Math::Vec4 kUniformColor{ k255(32), k255(96), k255(160), 1.f };
		const Rgba kExpected{ 32, 96, 160, 255 };

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta, which shares no channel with the expected colour, so a draw that never happened is
		// told apart from a draw that read the wrong bytes.
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		const auto buffer = makeUniformBuffer(rhi(), kUniformColor);
		ASSERT_TRUE(buffer);

		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		// range 0 means the whole buffer, which is what the shader's single vec4 block expects.
		write.buffers.push_back({ buffer, 0, 0 });
		rhi().updateDescriptorSets({ write });

		// The layout goes into the pipeline as well as into the set: on Vulkan it is what the pipeline
		// layout is built from, and binding a set against a pipeline that does not declare it is invalid.
		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_uniform.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &set, 1);
		ASSERT_FALSE(pixels.empty()) << "readback failed";

		// Every pixel, not one: the triangle covers the whole target, so a partial answer means the draw
		// was clipped rather than that the uniform was wrong, and that is worth telling apart.
		for (int y = 0; y < kSize; ++y) {
			for (int x = 0; x < kSize; ++x) {
				EXPECT_EQ(pixelAt(pixels, x, y), kExpected)
					<< "pixel (" << x << ", " << y << ") does not carry the uniform buffer's colour: the "
					   "value written through updateDescriptorSets did not reach the fragment shader";
			}
		}

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyBuffer(buffer);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// The second descriptor kind, and the one that carries the suite's orientation question. A
	// CombinedImageSampler binds two objects at once — a texture and a sampler — and a draw that
	// samples it composes three separate row-order conventions: how an upload lays source rows into
	// the texture, which way sampler V runs, and which way readback hands rows back. Each of those is
	// invisible on its own, so this test pins the composition and names it.
	TEST_P(RhiBackendTest, DescriptorCombinedImageSamplerReturnsTheTexelAtTheSampledUv) {
		// The same four colours, in the same order, as TextureInitialDataReadsBackUnchanged in
		// RhiResourceTests, so the two tests can be read side by side: that one pins create-then-read,
		// this one pins create-then-sample-then-read. Four distinct colours mean a vertical flip, a
		// transpose and a single wrong texel each fail differently.
		const std::vector<uint8_t> source = {
			255,   0,   0, 255,    0, 255,   0, 255,   // source row 0: red,  green
			  0,   0, 255, 255,  255, 255, 255, 255,   // source row 1: blue, white
		};
		const Rgba kRed{ 255, 0, 0, 255 };
		const Rgba kGreen{ 0, 255, 0, 255 };
		const Rgba kBlue{ 0, 0, 255, 255 };
		const Rgba kWhite{ 255, 255, 255, 255 };

		TextureDesc texDesc;
		texDesc.width = 2;
		texDesc.height = 2;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::Sampled | TextureUsage::TransferDst;
		const auto texture = rhi().createTexture(texDesc, source.data());
		ASSERT_TRUE(texture);

		// Nearest, and the target is 4x4, so every pixel centre lands at u or v of 0.125, 0.375, 0.625
		// or 0.875 — at least an eighth of the image away from the texel boundary at 0.5. The answer is
		// therefore one whole texel and never a blend of two, which is what keeps this test about the
		// descriptor binding rather than about filtering.
		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Nearest;
		samplerDesc.magFilter = FilterMode::Nearest;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		const auto sampler = rhi().createSampler(samplerDesc);
		ASSERT_TRUE(sampler);

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta shares no channel pattern with any of the four texels, so "the draw never ran" and
		// "the draw read the wrong texel" are different failures.
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		// One entry carrying both objects: the "combined" half of the type is exactly this, and a
		// backend that ignored either half would sample a default texture or with the wrong filter.
		write.images.push_back({ texture, sampler });
		rhi().updateDescriptorSets({ write });

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_sampler.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &set, 1);
		ASSERT_FALSE(pixels.empty()) << "readback failed";

		// Which way is up, spelled out, because this is the file that documents it.
		//
		// Three conventions compose here, and on both backends they compose to a vertical flip:
		//   1. The upload puts source row 0 at the V = 0 end of the texture (its "bottom" row), which
		//      is the same convention RhiResourceTests pins on the create-then-read round trip.
		//   2. rhi_test_fullscreen.vs sets vUv = clipPos * 0.5 + 0.5, so V = 1 at clip-space +Y, and
		//      RhiConventionTests has already pinned clip +Y as the top of the readback image.
		//   3. readbackTexture hands rows back top row first.
		// So V = 1 is the top of the image and reads the last source row: the image comes out with
		// source row 1 on top. Sampling therefore agrees with the plain round trip rather than
		// cancelling its flip — a texture uploaded and then sampled appears upside down relative to
		// the byte order it was written in. U is not flipped by any of the three: source column 0 stays
		// on the left.
		const Rgba expected[2][2] = {
			{ kBlue,  kWhite },   // image top row    <- source row 1
			{ kRed,   kGreen },   // image bottom row <- source row 0
		};

		// All sixteen pixels, not four probes: each 2x2 block of the target must carry one texel and
		// the same texel throughout, so a lookup that drifts across a texel boundary inside a quadrant
		// fails here instead of slipping past four lucky samples.
		for (int y = 0; y < kSize; ++y) {
			for (int x = 0; x < kSize; ++x) {
				const int quadY = y / (kSize / 2);
				const int quadX = x / (kSize / 2);
				EXPECT_EQ(pixelAt(pixels, x, y), expected[quadY][quadX])
					<< "pixel (" << x << ", " << y << ") sampled the wrong texel: the combined image "
					   "sampler did not resolve to the expected texel for this quadrant";
			}
		}

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// The sampler's own state, which the previous test deliberately kept out of the way by sampling
	// only at texel centres with Nearest. Filter mode and address mode are two enums that a backend
	// can mis-map without anything crashing: the wrong entry still produces a plausible colour, and
	// the only place the mistake is visible is a UV chosen so that the modes must disagree — on a
	// texel boundary, where Nearest and Linear cannot give the same answer, and outside [0, 1],
	// where Repeat and ClampToEdge cannot.
	TEST_P(RhiBackendTest, SamplerFilterAndAddressModeChangeTheSampledValue) {
		// Two texels whose average is exact in 8 bits: (64 + 192) / 2 is 128 with nothing left over,
		// so Linear at the midpoint is pinned to a whole byte and needs no tolerance. Grey rather than
		// four distinct colours on purpose — the two texels must not be a channel permutation of each
		// other, or a swizzle bug would turn one probe's answer into another probe's expectation.
		constexpr uint8_t kDarkByte = 64;
		constexpr uint8_t kLightByte = 192;
		constexpr uint8_t kMixByte = 128;
		const Rgba kDark{ kDarkByte, kDarkByte, kDarkByte, 255 };
		const Rgba kLight{ kLightByte, kLightByte, kLightByte, 255 };
		const Rgba kMix{ kMixByte, kMixByte, kMixByte, 255 };

		// Both rows identical, so V drops out of every answer below: whichever way the upload, the
		// sampler and the readback compose in V — the composition the previous test spells out — a
		// blend of two equal rows is that row, and a flip is invisible. That leaves U as the only axis
		// under test, which is what makes each expectation a statement about one enum.
		const std::vector<uint8_t> source = {
			kDarkByte, kDarkByte, kDarkByte, 255,   kLightByte, kLightByte, kLightByte, 255,
			kDarkByte, kDarkByte, kDarkByte, 255,   kLightByte, kLightByte, kLightByte, 255,
		};

		TextureDesc texDesc;
		texDesc.width = 2;
		texDesc.height = 2;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::Sampled | TextureUsage::TransferDst;
		const auto texture = rhi().createTexture(texDesc, source.data());
		ASSERT_TRUE(texture);

		const auto makeSampler = [&](FilterMode filter, AddressMode address) {
			SamplerDesc desc;
			desc.minFilter = filter;
			desc.magFilter = filter;
			desc.addressU = address;
			desc.addressV = address;
			desc.addressW = address;
			return rhi().createSampler(desc);
		};
		const auto nearestClamp = makeSampler(FilterMode::Nearest, AddressMode::ClampToEdge);
		const auto linearClamp = makeSampler(FilterMode::Linear, AddressMode::ClampToEdge);
		const auto nearestRepeat = makeSampler(FilterMode::Nearest, AddressMode::Repeat);
		ASSERT_TRUE(nearestClamp);
		ASSERT_TRUE(linearClamp);
		ASSERT_TRUE(nearestRepeat);

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Green: the one colour with a channel none of the three expected greys has, so "the draw never
		// ran" cannot be mistaken for "the sampler picked the wrong texel".
		const auto pass = makePass(rhi(), { 0.f, 1.f, 0.f, 1.f });
		ASSERT_TRUE(pass);

		// The sampled texel and the UV to sample it at, in one set: the UV has to be a value the test
		// picks rather than the interpolated vUv, which only ever covers pixel centres inside [0, 1].
		// One set, so OpenGL discarding the set index cannot matter, and binding 1 rather than 15,
		// which SFE_PUSH_BLOCK reserves there.
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		layoutDesc.bindings.push_back({ 1, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_sampler_uv.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		// One draw per probe, each through its own freshly allocated set: a probe must not depend on
		// what an earlier probe wrote, and rewriting a live set is a separate question this test is not
		// asking. The whole target is one colour because the UV is a uniform, so any pixel answers —
		// but all sixteen are checked, so a clipped draw fails here rather than passing on a lucky one.
		const auto sampleAt = [&](SamplerHandle sampler, float u) -> Rgba {
			const auto set = rhi().allocateDescriptorSet(layout);
			EXPECT_TRUE(set);
			const auto uvBuffer = makeUniformBuffer(rhi(), { u, 0.5f, 0.f, 0.f });
			EXPECT_TRUE(uvBuffer);

			WriteDescriptor image{};
			image.set = set;
			image.binding = 0;
			image.images.push_back({ texture, sampler });
			WriteDescriptor uv{};
			uv.set = set;
			uv.binding = 1;
			uv.buffers.push_back({ uvBuffer, 0, 0 });
			rhi().updateDescriptorSets({ image, uv });

			const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &set, 1);
			EXPECT_FALSE(pixels.empty()) << "readback failed";
			const Rgba first = pixelAt(pixels, 0, 0);
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), first)
						<< "pixel (" << x << ", " << y << ") differs from the rest although every fragment "
						   "sampled the same uniform UV: the draw did not cover the target";
				}
			}

			rhi().destroyBuffer(uvBuffer);
			rhi().freeDescriptorSet(set);
			return first;
		};

		// Texel centres of a 2-wide texture sit at u = 0.25 and u = 0.75; the boundary between them is
		// u = 0.5. Nearest is probed a sixty-fourth to either side rather than on the boundary itself,
		// because exactly on it the two texels are equidistant and which one wins is a tie the spec
		// leaves to the implementation — an answer both backends may legitimately give differently.
		// A sixty-fourth is an exact binary fraction, so both probes land on the side they were meant
		// to and no float rounding decides the result.
		EXPECT_EQ(sampleAt(nearestClamp, 0.484375f), kDark)
			<< "Nearest just left of the texel boundary did not return the left texel whole: the sampler "
			   "blended rather than snapping, so minFilter/magFilter Nearest was not honoured";
		EXPECT_EQ(sampleAt(nearestClamp, 0.515625f), kLight)
			<< "Nearest just right of the texel boundary did not return the right texel whole";

		// Linear, in contrast, is probed exactly on the boundary, where the two texels weigh 0.5 each
		// and the answer is their average. That is a value Nearest cannot produce from any UV, so this
		// one expectation is what separates the two filter modes.
		EXPECT_EQ(sampleAt(linearClamp, 0.5f), kMix)
			<< "Linear on the texel boundary did not return the average of the two texels: the two "
			   "neighbours were not blended, so FilterMode::Linear behaved as Nearest";

		// Outside [0, 1], where the address mode is the whole answer. u = 1.25 is a quarter of a
		// texture past the right edge: ClampToEdge holds the last texel, Repeat wraps to u = 0.25,
		// which is the left texel's centre. The two modes therefore name different texels, and the
		// pixel tells them apart with no shared value to hide a mis-mapped enum behind.
		EXPECT_EQ(sampleAt(nearestClamp, 1.25f), kLight)
			<< "ClampToEdge at u = 1.25 did not return the right-hand edge texel: the U coordinate was "
			   "not clamped to the edge";
		EXPECT_EQ(sampleAt(nearestRepeat, 1.25f), kDark)
			<< "Repeat at u = 1.25 did not return the texel at u = 0.25: the U coordinate did not wrap, "
			   "so AddressMode::Repeat was not honoured";

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroySampler(nearestRepeat);
		rhi().destroySampler(linearClamp);
		rhi().destroySampler(nearestClamp);
		rhi().destroyTexture(texture);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// The third descriptor kind. A uniform buffer is one small block the backend can afford to copy
	// anywhere; a storage buffer is an array the shader indexes at run time, so it pins two things the
	// uniform test cannot reach: that indexing works at all — an SSBO that returned element 0 for every
	// index would pass a one-element test — and that the shader's std430 packing agrees with the plain
	// C++ float array the test uploaded. Under std140 an array of float has a stride of 16 bytes, so a
	// backend that fed this layout as std140 would read every fourth element and fail here rather than
	// silently in a real shader.
	TEST_P(RhiBackendTest, DescriptorStorageBufferIsIndexableFromTheFragmentShader) {
		// Sixteen elements, one per pixel of the target, each byte a whole multiple of 17 so the float
		// the shader writes lands on one exact byte and no tolerance is needed: 0, 17, 34 ... 255. All
		// sixteen differ, so a wrong index is a wrong colour rather than a lucky match.
		constexpr int kCount = kSize * kSize;
		std::vector<float> values(kCount);
		for (int i = 0; i < kCount; ++i) {
			values[i] = k255(i * 17);
		}

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Blue is the one colour the shader never writes — it forces B to zero — so a draw that never
		// happened is told apart from a draw that read the wrong elements.
		const auto pass = makePass(rhi(), { 0.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		BufferDesc bufferDesc;
		bufferDesc.size = values.size() * sizeof(float);
		bufferDesc.usage = BufferUsage::Storage;
		bufferDesc.cpuAccess = CpuAccess::Write;
		bufferDesc.debugName = "rhi_test_descriptor_storage";
		const auto buffer = rhi().createBuffer(bufferDesc, values.data());
		ASSERT_TRUE(buffer);

		// One binding and no uniform buffer beside it. OpenGL binds storage buffers into their own
		// GL_SHADER_STORAGE_BUFFER binding-point namespace while Vulkan has one namespace per set, so a
		// set mixing the two kinds would be asking a second question; binding 0, and not 15, which
		// SFE_PUSH_BLOCK reserves on OpenGL.
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::StorageBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		// range 0 means the whole buffer, which is what an unsized array in the shader block expects.
		write.buffers.push_back({ buffer, 0, 0 });
		rhi().updateDescriptorSets({ write });

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_storage.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &set, 1);
		ASSERT_FALSE(pixels.empty()) << "readback failed";

		for (int y = 0; y < kSize; ++y) {
			for (int x = 0; x < kSize; ++x) {
				// The shader's row comes from vUv.y, and vUv = clipPos * 0.5 + 0.5 with clip +Y at the
				// top of the readback image — the composition the sampler test above spells out. So
				// vUv.y = 1 is the image's top row and reads the last row of elements: image row y holds
				// buffer row kSize - 1 - y. U is not flipped, so the column is the pixel's own x.
				const int idx = (kSize - 1 - y) * kSize + x;
				const Rgba expected{
					static_cast<uint8_t>(idx * 17),
					static_cast<uint8_t>((kCount - 1 - idx) * 17),
					0,
					255,
				};
				EXPECT_EQ(pixelAt(pixels, x, y), expected)
					<< "pixel (" << x << ", " << y << ") did not read element " << idx << " and element "
					<< (kCount - 1 - idx) << " of the storage buffer: either the SSBO binding did not "
					   "reach the fragment shader, the shader is not indexing the array, or the "
					   "shader's std430 packing disagrees with the float array the test uploaded";
			}
		}

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyBuffer(buffer);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// A descriptor set is not write-once. Every test above writes a set once and then only reads it,
	// which leaves the whole question of rewriting untested — and rewriting is what a real frame does:
	// one set per material slot, repointed at a different buffer for every object. The two backends
	// reach that from opposite directions. OpenGL has no descriptor sets at all: a set is a CPU-side
	// list of bindings that cmdBindDescriptorSets copies into the command list, so a rewrite is a
	// plain vector assignment. Vulkan writes a live VkDescriptorSet the GPU may already be holding.
	// So the same two lines of caller code mean two very different things, and this test is what says
	// they must still produce the same pixels.
	TEST_P(RhiBackendTest, DescriptorSetRewriteChangesWhatTheNextDrawReads) {
		// Two colours that share no channel value, so pointing at the wrong buffer can never coincide
		// with pointing at the right one, and each is a whole 1/255 step so no tolerance is needed.
		const Math::Vec4 kColorA{ k255(32), k255(96), k255(160), 1.f };
		const Math::Vec4 kColorB{ k255(200), k255(16), k255(64), 1.f };
		const Rgba kExpectedA{ 32, 96, 160, 255 };
		const Rgba kExpectedB{ 200, 16, 64, 255 };

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta again: it shares no channel with either colour, so "the draw never ran" and "the draw
		// read the other buffer" are three distinguishable answers rather than two.
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		// One set, one binding, and binding 0 rather than 15, which SFE_PUSH_BLOCK reserves on OpenGL.
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		// Exactly one set for the whole test, allocated once and never reallocated: that a *rewrite*
		// took effect, rather than a fresh set being bound, is the entire claim, so a second
		// allocateDescriptorSet anywhere below would quietly answer a different question.
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		const auto bufferA = makeUniformBuffer(rhi(), kColorA);
		const auto bufferB = makeUniformBuffer(rhi(), kColorB);
		ASSERT_TRUE(bufferA);
		ASSERT_TRUE(bufferB);

		// Both writes name the same set and the same binding and differ only in the buffer, which is
		// what makes the second one a rewrite rather than a write to a free slot.
		WriteDescriptor writeA{};
		writeA.set = set;
		writeA.binding = 0;
		writeA.buffers.push_back({ bufferA, 0, 0 });
		WriteDescriptor writeB = writeA;
		writeB.buffers.clear();
		writeB.buffers.push_back({ bufferB, 0, 0 });

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_uniform.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto expectEveryPixel = [&](const std::vector<uint8_t>& pixels, const Rgba& expected,
			const char* what) {
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< "pixel (" << x << ", " << y << "): " << what;
				}
			}
		};

		// Frame one, buffer A. This is the half that stops the rest of the test being vacuous: without
		// it, a backend that ignored both writes and left the binding empty could still "change" nothing
		// and no expectation below would notice.
		rhi().updateDescriptorSets({ writeA });
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1), kExpectedA,
			"the first write did not reach the fragment shader");

		// Frame two, the same set repointed at buffer B. drawWithSets ends its frame with present() and
		// waitIdle(), so the first frame is fully retired before this write happens — the easy case, and
		// the only one this RHI can actually express (see below).
		rhi().updateDescriptorSets({ writeB });
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1), kExpectedB,
			"the set still reads buffer A after being rewritten to buffer B: a descriptor set is being "
			"treated as write-once, or the second updateDescriptorSets was dropped");

		// There is deliberately no third case rewriting a set between two draws in the *same* command
		// list. It looks like the interesting one, and it is invalid: Vulkan says updating a descriptor
		// set that is bound into a command buffer in the recording state invalidates that command
		// buffer, unless the set was allocated UPDATE_AFTER_BIND, which this RHI never does. A test
		// doing it recorded a frame the validation layer rejects
		// ("VkDescriptorSet ... was destroy or updated without UPDATE_AFTER_BIND"), and every later
		// vkCmd* in that frame cascades off it. It passed anyway, which is exactly why it was worth
		// removing rather than keeping.
		//
		// The Vulkan rule the interface states nowhere: vkUpdateDescriptorSets must not touch a set
		// that a recording *or* submitted-and-still-executing command buffer uses. The caller must
		// rewrite between frames. Nothing in RenderHardwareInterface.h says so, and OpenGL does not
		// care, so the trap is invisible until validation is on.
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyBuffer(bufferB);
		rhi().destroyBuffer(bufferA);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// More sets than one Vulkan descriptor pool holds (2048). Running out used to return empty handles with no
	// message, and a bind with one silently kept the previous draw's textures.
	TEST_P(RhiBackendTest, DescriptorSetsOutliveOnePool) {
		const Math::Vec4 kColor{ k255(32), k255(96), k255(160), 1.f };
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);
		const auto buffer = makeUniformBuffer(rhi(), kColor);
		ASSERT_TRUE(buffer);

		std::vector<DescriptorSetHandle> sets;
		for (int i = 0; i < 3000; ++i) {
			const auto set = rhi().allocateDescriptorSet(layout);
			ASSERT_TRUE(set) << "set " << i << " could not be allocated";
			sets.push_back(set);
		}
		// The last one, from a later pool, still reaches the shader.
		rhi().updateDescriptorSets({ { .set = sets.back(), .binding = 0, .buffers{ { buffer, 0, 0 } } } });
		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_uniform.fs", { layout });
		ASSERT_TRUE(pipeline);
		const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &sets.back(), 1);
		ASSERT_FALSE(pixels.empty()) << "readback failed";
		EXPECT_EQ(pixelAt(pixels, 1, 1), (Rgba{ 32, 96, 160, 255 }));

		rhi().destroyGraphicsPipeline(pipeline);
		for (const auto set : sets) {
			rhi().freeDescriptorSet(set);
		}
		rhi().destroyBuffer(buffer);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// Per-frame data through allocTransient and a dynamic uniform binding: the set is written once, and
	// each draw picks its allocation by the offset passed at bind. Two allocations in one frame must not
	// overlap, the offset must reach the shader on both backends, and a later frame's allocation must be
	// readable through the same unchanged set.
	TEST_P(RhiBackendTest, TransientAllocationsReachTheShaderThroughADynamicOffset) {
		const Math::Vec4 kColorA{ k255(32), k255(96), k255(160), 1.f };
		const Math::Vec4 kColorB{ k255(200), k255(16), k255(64), 1.f };
		const Math::Vec4 kColorC{ k255(8), k255(240), k255(120), 1.f };

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::UniformBufferDynamic, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);
		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_uniform.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto expectEveryPixel = [&](const std::vector<uint8_t>& pixels, const Rgba& expected, const char* what) {
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected) << "pixel (" << x << ", " << y << "): " << what;
				}
			}
		};

		// Frame one: two allocations, drawn through the second so the offset is not zero.
		const auto a = rhi().allocTransient(kColorA);
		const auto b = rhi().allocTransient(kColorB);
		ASSERT_TRUE(a);
		ASSERT_TRUE(b);
		ASSERT_EQ(a.buffer.id, b.buffer.id) << "two small allocations of one frame should share the transient buffer";
		EXPECT_NE(a.offset, b.offset);
		rhi().updateDescriptorSets({ { .set = set, .binding = 0, .buffers{ { a.buffer, 0, sizeof(Math::Vec4) } } } });
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1, 0, &b.offset, 1), { 200, 16, 64, 255 },
			"the second allocation did not reach the shader at its dynamic offset");

		// Frame two: a new allocation through the set written in frame one.
		const auto c = rhi().allocTransient(kColorC);
		ASSERT_TRUE(c);
		ASSERT_EQ(c.buffer.id, a.buffer.id) << "the transient buffer changed, so the set written for it would be stale";
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1, 0, &c.offset, 1), { 8, 240, 120, 255 },
			"a later frame's allocation did not reach the shader through the unchanged set");

		// Frame three: two allocations again, drawn through the first, so the second did not overwrite it.
		const auto d = rhi().allocTransient(kColorA);
		const auto e = rhi().allocTransient(kColorB);
		ASSERT_TRUE(d);
		ASSERT_TRUE(e);
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1, 0, &d.offset, 1), { 32, 96, 160, 255 },
			"the first allocation was overwritten by the second");

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// The rest of AddressMode. SamplerFilterAndAddressModeChangeTheSampledValue pins Repeat and
	// ClampToEdge; the other three enumerators are mapped by both backends but reached by nothing, so
	// a swapped pair in either switch — MirrorRepeat written where MirrorClampToEdge belongs, say —
	// would compile, run and produce a plausible picture. The two mirror modes are only ever different
	// from each other, and from the two already pinned, at UVs chosen so their wrapping rules disagree,
	// which is what this test does and all it does.
	//
	// The rule both specs state, and the one worth having written down here because it is the opposite
	// of what the name suggests: MirrorClampToEdge mirrors only about zero. Its texel wrap is
	// clamp(mirror(i), 0, size - 1) with mirror(a) = a for a >= 0 and -(1 + a) below, so on the
	// positive side it is plain ClampToEdge and the "mirror once, then clamp" behaviour lives entirely
	// at negative U. Every probe below is placed by that formula.
	TEST_P(RhiBackendTest, SamplerMirrorAddressModesWrapAsTheSpecDescribes) {
		// The same two-texel greyscale texture as the filter test, and for the same reason: both rows
		// identical so V drops out and U is the only axis under test, and two greys rather than two
		// colours so a channel swizzle cannot turn one probe's answer into another probe's expectation.
		constexpr uint8_t kDarkByte = 64;
		constexpr uint8_t kLightByte = 192;
		const Rgba kDark{ kDarkByte, kDarkByte, kDarkByte, 255 };
		const Rgba kLight{ kLightByte, kLightByte, kLightByte, 255 };

		const std::vector<uint8_t> source = {
			kDarkByte, kDarkByte, kDarkByte, 255,   kLightByte, kLightByte, kLightByte, 255,
			kDarkByte, kDarkByte, kDarkByte, 255,   kLightByte, kLightByte, kLightByte, 255,
		};

		TextureDesc texDesc;
		texDesc.width = 2;
		texDesc.height = 2;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::Sampled | TextureUsage::TransferDst;
		const auto texture = rhi().createTexture(texDesc, source.data());
		ASSERT_TRUE(texture);

		const auto makeSampler = [&](AddressMode address) {
			SamplerDesc desc;
			// Nearest throughout: a blend of two texels would put filtering inside an answer that is
			// meant to be about the wrap rule alone.
			desc.minFilter = FilterMode::Nearest;
			desc.magFilter = FilterMode::Nearest;
			desc.addressU = address;
			desc.addressV = address;
			desc.addressW = address;
			return rhi().createSampler(desc);
		};
		const auto mirrorRepeat = makeSampler(AddressMode::MirrorRepeat);
		const auto mirrorClamp = makeSampler(AddressMode::MirrorClampToEdge);
		const auto clampToBorder = makeSampler(AddressMode::ClampToBorder);
		ASSERT_TRUE(mirrorRepeat);
		ASSERT_TRUE(mirrorClamp);
		ASSERT_TRUE(clampToBorder);

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Green: the one colour with a channel neither grey has, so "the draw never ran" cannot be
		// mistaken for "the sampler wrapped to the wrong texel".
		const auto pass = makePass(rhi(), { 0.f, 1.f, 0.f, 1.f });
		ASSERT_TRUE(pass);

		// One set, binding 0 and 1 — not 15, which SFE_PUSH_BLOCK reserves on OpenGL — because OpenGL
		// discards the set index entirely and a second set would be asking a different question.
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		layoutDesc.bindings.push_back({ 1, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_sampler_uv.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		// One draw per probe through its own freshly allocated set, so no probe can inherit what an
		// earlier one wrote. V is 0.5 in every probe: inside [0, 1] for every mode here, so the address
		// mode never touches it and the answer is a statement about U only.
		const auto sampleAt = [&](SamplerHandle sampler, float u) -> Rgba {
			const auto set = rhi().allocateDescriptorSet(layout);
			EXPECT_TRUE(set);
			const auto uvBuffer = makeUniformBuffer(rhi(), { u, 0.5f, 0.f, 0.f });
			EXPECT_TRUE(uvBuffer);

			WriteDescriptor image{};
			image.set = set;
			image.binding = 0;
			image.images.push_back({ texture, sampler });
			WriteDescriptor uv{};
			uv.set = set;
			uv.binding = 1;
			uv.buffers.push_back({ uvBuffer, 0, 0 });
			rhi().updateDescriptorSets({ image, uv });

			const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &set, 1);
			EXPECT_FALSE(pixels.empty()) << "readback failed";
			const Rgba first = pixelAt(pixels, 0, 0);
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), first)
						<< "pixel (" << x << ", " << y << ") differs from the rest although every fragment "
						   "sampled the same uniform UV: the draw did not cover the target";
				}
			}

			rhi().destroyBuffer(uvBuffer);
			rhi().freeDescriptorSet(set);
			return first;
		};

		// Every U below is a half-integer in texel space (u * 2), which is the exact centre of a texel
		// and as far from a texel boundary as the coordinate can get, so no rounding rule and no
		// residual filtering can decide any of these answers.
		//
		// The four modes' answers at the four probes, from the wrap formulas, with i = floor(u * 2):
		//
		//                        u=1.25   u=2.25   u=-0.75   u=-2.25
		//   Repeat               dark     dark     dark      light
		//   ClampToEdge          light    light    dark      dark
		//   MirrorRepeat         light    dark     light     dark
		//   MirrorClampToEdge    light    light    light     light
		//
		// Every row is distinct, so each mode below is pinned to a signature no other AddressMode can
		// produce — which is the only way a mis-mapped enumerator is caught rather than hidden behind a
		// value two modes happen to share.

		// MirrorRepeat: the tile at [1, 2] is reflected, so u = 1.25 reads the texel at u = 0.75 rather
		// than the u = 0.25 texel plain Repeat would wrap to.
		EXPECT_EQ(sampleAt(mirrorRepeat, 1.25f), kLight)
			<< "MirrorRepeat at u = 1.25 did not return the texel at u = 0.75: the first tile past the "
			   "edge was not reflected, so the mode behaved as Repeat";
		// And the tile after that is the right way round again, which is what separates MirrorRepeat
		// from the two clamping modes: they both hold the right-hand texel out here forever.
		EXPECT_EQ(sampleAt(mirrorRepeat, 2.25f), kDark)
			<< "MirrorRepeat at u = 2.25 did not return the texel at u = 0.25: the second tile past the "
			   "edge was reflected as well, or the coordinate was clamped instead of repeating";

		// MirrorClampToEdge, on the negative side, where its mirror actually lives. u = -0.75 reflects
		// about zero to 0.75 and reads the right-hand texel — ClampToEdge would hold the left-hand one.
		EXPECT_EQ(sampleAt(mirrorClamp, -0.75f), kLight)
			<< "MirrorClampToEdge at u = -0.75 did not reflect about zero to the texel at u = 0.75: the "
			   "coordinate was clamped without mirroring, so the mode behaved as ClampToEdge";
		// Past one full texture width the reflection is over and the clamp takes it: u = -2.25 does not
		// mirror a second time the way MirrorRepeat would, it holds the far texel.
		EXPECT_EQ(sampleAt(mirrorClamp, -2.25f), kLight)
			<< "MirrorClampToEdge at u = -2.25 mirrored a second time instead of clamping: the mode "
			   "behaved as MirrorRepeat";
		// On the positive side it is plain ClampToEdge, which is the half of the rule the name hides.
		EXPECT_EQ(sampleAt(mirrorClamp, 2.25f), kLight)
			<< "MirrorClampToEdge at u = 2.25 did not hold the right-hand edge texel: positive U is not "
			   "mirrored by this mode at all, it is clamped";

		// ClampToBorder is deliberately not asserted. SamplerDesc has no border-colour field, so what a
		// sampler clamps to is whatever the backend happens to leave in place, and the two do not agree:
		// Vulkan hardcodes VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK when it fills VkSamplerCreateInfo, while
		// OpenGL never touches GL_TEXTURE_BORDER_COLOR and so inherits GL's default of transparent
		// black. Pinning either number would be pinning a backend default the caller never asked for
		// and cannot currently set. The value is printed instead, so a change in either default shows up
		// in the log rather than passing unseen.
		const Rgba border = sampleAt(clampToBorder, 1.25f);
		std::cout << "[ INFO     ] ClampToBorder at u = 1.25 returned RGBA("
			<< static_cast<int>(border.r) << ", " << static_cast<int>(border.g) << ", "
			<< static_cast<int>(border.b) << ", " << static_cast<int>(border.a)
			<< ") — a backend default, not a value SamplerDesc can express\n";

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroySampler(clampToBorder);
		rhi().destroySampler(mirrorClamp);
		rhi().destroySampler(mirrorRepeat);
		rhi().destroyTexture(texture);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// DescriptorSetLayoutBinding::count, the one field of the descriptor path that every test above
	// leaves at its default. An array binding is a single slot carrying several elements, which the
	// shader reads as tex[0], tex[1], tex[2] — and both backends used to deliver only the first, in
	// two different and equally silent ways. OpenGL bound images[0] to the unit equal to the binding
	// number and left the rest of the range unbound, so the later elements sampled black and the frame
	// looked merely wrong. Vulkan declared the array in the layout but wrote descriptorCount = 1, so
	// the tail was never written and the first draw that read it **lost the device**, after which
	// submit, present and readbackTexture all keep reporting success while handing back zeros. So this
	// test is not only "does the array arrive": it is the one that says an array binding is not a way
	// to take the driver down without a word.
	//
	// Three elements, not two. An off-by-one that bound elements 0 and 1 correctly and dropped the
	// last is exactly the mistake a two-element test cannot see.
	TEST_P(RhiBackendTest, DescriptorArrayBindingDeliversEveryElement) {
		// Solid textures, one grey level each. With every texel of an image equal, the three-way
		// composition of upload order, sampler V and readback order that
		// DescriptorCombinedImageSamplerReturnsTheTexelAtTheSampledUv works out drops out of this test
		// entirely — a flip or a transpose cannot change a constant image. That leaves array indexing
		// as the only thing left to fail on, which is the whole point of choosing solid colours here.
		// Three distinct whole bytes, so element 0 standing in for element 1 is a visibly different
		// answer rather than a coincidence.
		constexpr uint8_t kTexByte[3] = { 32, 96, 160 };
		const Rgba kExpectedImages{ kTexByte[0], kTexByte[1], kTexByte[2], 255 };

		const auto makeSolidTexture = [&](uint8_t value) {
			// All four channels the same value: the shader reads only .r, so the rest is filler, and a
			// 2x2 rather than a 1x1 keeps the upload on the same shape every other test in this file
			// uses.
			const std::vector<uint8_t> texels(2 * 2 * 4, value);
			TextureDesc texDesc;
			texDesc.width = 2;
			texDesc.height = 2;
			texDesc.depth = 1;
			texDesc.arrayLayers = 1;
			texDesc.dimension = TextureDimension::Tex2D;
			texDesc.format = TextureFormat::RGBA8;
			texDesc.usage = TextureUsage::Sampled | TextureUsage::TransferDst;
			return rhi().createTexture(texDesc, texels.data());
		};
		const auto textureA = makeSolidTexture(kTexByte[0]);
		const auto textureB = makeSolidTexture(kTexByte[1]);
		const auto textureC = makeSolidTexture(kTexByte[2]);
		ASSERT_TRUE(textureA);
		ASSERT_TRUE(textureB);
		ASSERT_TRUE(textureC);

		// One sampler shared by all three elements: what varies between array elements here is the
		// texture, so anything else varying would blur what a wrong pixel means.
		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Nearest;
		samplerDesc.magFilter = FilterMode::Nearest;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		const auto sampler = rhi().createSampler(samplerDesc);
		ASSERT_TRUE(sampler);

		// Two channels out of each uniform block, so both elements of the buffer array are pinned and a
		// missing second block cannot be mistaken for a wrong value in the first. The last channel is
		// 1.0 so the pixel's alpha is 255: an element that never arrived reads as zero on OpenGL, which
		// is then a different alpha as well as a different blue.
		const Math::Vec4 kBlockA{ k255(24), k255(72), 0.f, 0.f };
		const Math::Vec4 kBlockB{ k255(136), 1.f, 0.f, 0.f };
		const Rgba kExpectedBuffers{ 24, 72, 136, 255 };

		const auto bufferA = makeUniformBuffer(rhi(), kBlockA);
		const auto bufferB = makeUniformBuffer(rhi(), kBlockB);
		ASSERT_TRUE(bufferA);
		ASSERT_TRUE(bufferB);

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta shares no channel value with either expected pixel, so "the draw never ran" — which
		// is also what a lost device looks like once the readback starts returning the clear or zeros —
		// is told apart from "the array came back short".
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		// One layout and one set for both halves. The two arrays are the only thing under test, so the
		// binding numbers are chosen to keep every other known hazard out of the way: binding 0 with
		// count 3 occupies OpenGL texture units 0..2 and binding 4 with count 2 occupies UBO points
		// 4..5, so neither range reaches 15, which SFE_PUSH_BLOCK reserves on OpenGL, and the two
		// ranges cannot overlap each other. Distinct binding numbers also keep Vulkan from splitting
		// the layout into a separate image set and buffer set, which same-number bindings of different
		// kinds would trigger.
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 3 });
		layoutDesc.bindings.push_back({ 4, DescriptorType::UniformBuffer, ShaderStage::Fragment, 2 });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		// One write per binding, carrying the whole array from element 0 upwards: WriteDescriptor has
		// no dstArrayElement, so that is the only shape the interface can express, and the order of
		// this vector is the array order the shader indexes.
		WriteDescriptor images{};
		images.set = set;
		images.binding = 0;
		images.images.push_back({ textureA, sampler });
		images.images.push_back({ textureB, sampler });
		images.images.push_back({ textureC, sampler });
		WriteDescriptor buffers{};
		buffers.set = set;
		buffers.binding = 4;
		buffers.buffers.push_back({ bufferA, 0, 0 });
		buffers.buffers.push_back({ bufferB, 0, 0 });
		rhi().updateDescriptorSets({ images, buffers });

		// Two pipelines against the same layout, because one pixel has four channels and the two arrays
		// carry five values between them. A pipeline layout may declare a binding its shader never
		// reads, so each shader sees only its own array while the set stays whole — and on Vulkan every
		// binding the layout declares is written, which is what keeps the unread one from faulting.
		const auto imagePipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_sampler_array.fs", { layout });
		ASSERT_TRUE(imagePipeline) << "pipeline creation failed";
		const auto bufferPipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_uniform_array.fs", { layout });
		ASSERT_TRUE(bufferPipeline) << "pipeline creation failed";

		const auto expectEveryPixel = [&](const std::vector<uint8_t>& pixels, const Rgba& expected,
			const char* what) {
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< "pixel (" << x << ", " << y << "): " << what;
				}
			}
		};

		// The image array. Red carries element 0, green element 1, blue element 2, so a short array
		// leaves a zero channel that names which element went missing.
		expectEveryPixel(drawWithSets(rhi(), target, pass, imagePipeline, &set, 1), kExpectedImages,
			"the three textures written into one CombinedImageSampler array did not all reach the "
			"fragment shader: a zero channel means that element's texture was never bound, so the "
			"array descriptor was collapsed to fewer elements than it was written with");

		// The buffer array, into the same target — makePass clears, so the previous draw cannot leak
		// into this one.
		expectEveryPixel(drawWithSets(rhi(), target, pass, bufferPipeline, &set, 1), kExpectedBuffers,
			"the two buffers written into one UniformBuffer array did not both reach the fragment "
			"shader: blue and alpha come from the second element, so zeros there mean the array was "
			"truncated to its first buffer");

		rhi().destroyGraphicsPipeline(bufferPipeline);
		rhi().destroyGraphicsPipeline(imagePipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyBuffer(bufferB);
		rhi().destroyBuffer(bufferA);
		rhi().destroySampler(sampler);
		rhi().destroyTexture(textureC);
		rhi().destroyTexture(textureB);
		rhi().destroyTexture(textureA);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// The caller getting it wrong. Every test above hands updateDescriptorSets a WriteDescriptor whose
	// type agrees with the layout's; this one hands it two that do not, and asserts that neither is
	// allowed to reach the driver. The mistake used to split the backends in the worst possible
	// direction. OpenGL matched its internal slot on binding *and* type, so a disagreeing write found
	// no slot and appended a second one: the original stayed authoritative, the rewrite was silently
	// invisible, and the next draw returned the value the caller believed it had just replaced — no
	// error, no warning. Vulkan passed the caller's type to vkUpdateDescriptorSets, the driver read
	// pImageInfo where the layout promised a buffer, and the process took an access violation
	// (0xC0000005) behind VUID-VkWriteDescriptorSet-descriptorType-00319. So a caller developing
	// against OpenGL would have shipped a Vulkan crash, which is exactly the class of divergence this
	// suite exists to catch.
	//
	// Both backends now take the type from the set's own layout, and `WriteDescriptor` no longer
	// carries a type at all -- the disagreement is unexpressible rather than merely detected, which is
	// why the wrong-type half of this test is gone. What survives is the branch a caller can still
	// reach: a write aimed at a binding the layout never declared.
	TEST_P(RhiBackendTest, DescriptorWriteToAnUndeclaredBindingIsRejected) {
		// Two colours sharing no channel value, each a whole 1/255 step, so "the set kept A" and "the
		// set took B" can never coincide and neither needs a tolerance.
		const Math::Vec4 kColorA{ k255(32), k255(96), k255(160), 1.f };
		const Math::Vec4 kColorB{ k255(200), k255(16), k255(64), 1.f };
		const Rgba kExpectedA{ 32, 96, 160, 255 };
		const Rgba kExpectedB{ 200, 16, 64, 255 };

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta shares no channel with either colour, so a draw that never ran is a third
		// distinguishable answer rather than being mistaken for one of the two.
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		// One binding, declared UniformBuffer. Binding 0 rather than 15, which SFE_PUSH_BLOCK reserves
		// on OpenGL. The layout deliberately declares nothing else: binding 1 below is undeclared, which
		// is what makes the second half of the guard reachable.
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		// One set for the whole test, allocated once. The claim is about what a *rejected* write does to
		// a set that already holds something, so a second allocateDescriptorSet anywhere below would
		// quietly answer a different question.
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		const auto bufferA = makeUniformBuffer(rhi(), kColorA);
		const auto bufferB = makeUniformBuffer(rhi(), kColorB);
		ASSERT_TRUE(bufferA);
		ASSERT_TRUE(bufferB);

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_uniform.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto expectEveryPixel = [&](const std::vector<uint8_t>& pixels, const Rgba& expected,
			const char* what) {
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< "pixel (" << x << ", " << y << "): " << what;
				}
			}
		};

		// The valid write first, and it is what keeps the rest from being vacuous. The assertion that
		// the set did *not* change passes just as happily when nothing ever worked — when the binding
		// was empty all along and the draw was reading zeros or the clear. This frame is the evidence
		// that the set really does hold colour A when the rejected write arrives, so "unchanged"
		// afterwards means something.
		WriteDescriptor valid{};
		valid.set = set;
		valid.binding = 0;
		valid.buffers.push_back({ bufferA, 0, 0 });
		rhi().updateDescriptorSets({ valid });
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1), kExpectedA,
			"the first, valid write did not reach the fragment shader, so the rejection assertions "
			"below would prove nothing");

		// A binding the layout never declared. The write is otherwise entirely valid — live buffer, the
		// same shape as the accepted one above — so only the binding number can be what rejects it.
		WriteDescriptor undeclaredBinding{};
		undeclaredBinding.set = set;
		undeclaredBinding.binding = 1;
		undeclaredBinding.buffers.push_back({ bufferB, 0, 0 });
		rhi().updateDescriptorSets({ undeclaredBinding });

		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1), kExpectedA,
			"binding 0 changed after a write aimed at binding 1, which the layout does not declare: an "
			"out-of-layout write must be dropped, not applied to some other slot");

		// And the set is still usable afterwards. A guard that rejected the two bad writes by wedging
		// the set — or by leaving Vulkan's descriptor pool in a state the next vkUpdateDescriptorSets
		// cannot recover from — would satisfy every expectation above and still be broken, so the test
		// ends by proving that a valid rewrite following two rejected ones takes effect normally.
		WriteDescriptor rewrite = valid;
		rewrite.buffers.clear();
		rewrite.buffers.push_back({ bufferB, 0, 0 });
		rhi().updateDescriptorSets({ rewrite });
		expectEveryPixel(drawWithSets(rhi(), target, pass, pipeline, &set, 1), kExpectedB,
			"a valid rewrite after a rejected write did not take effect: the rejection left the set "
			"unwritable rather than merely unchanged");

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyBuffer(bufferB);
		rhi().destroyBuffer(bufferA);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// The set index itself, which every test above leaves at zero. cmdBindDescriptorSets takes an
	// array of sets and a firstSet offset, and nothing in the suite has ever passed more than one set
	// or an offset other than 0 — so the parameter that names *where* a set lands has been carried by
	// the interface and exercised by nothing.
	//
	// The two backends mean opposite things by it. Vulkan has one binding namespace per set: the
	// pipeline layout is an ordered list of set layouts, a set bound at index i is addressed as
	// set = i in the shader, and binding numbers only have to be unique within a set.
	// OpenGL::cmdBindDescriptorSets names neither the set index nor firstSet — both parameters are
	// unnamed in the signature — and flattens every bound set's bindings into one list that
	// execBindDescriptorSets replays by binding number alone: glBindBufferBase(..., b.binding, ...),
	// texture unit = b.binding. sfe.glsl matches that by making SFE_SET drop the set index outside
	// Vulkan. So on OpenGL a set index is not an address, it is nothing at all, and the binding number
	// is the whole address.
	//
	// That leaves exactly one shape both backends can express, and this test pins it: sets whose
	// binding numbers do not collide. See the note at the end of this test for the shape they cannot.
	TEST_P(RhiBackendTest, TwoDescriptorSetsBoundAtOnceEachReachItsOwnBinding) {
		// Two channels out of each set, so both are pinned in a single pixel and a set that never
		// arrived leaves zeros in its own pair of channels rather than being mistaken for a wrong value
		// in the other. All four bytes differ, so swapping the two sets is a different answer again.
		const Math::Vec4 kSetZeroColor{ k255(24), k255(72), 0.f, 0.f };
		const Math::Vec4 kSetOneColor{ k255(136), k255(200), 0.f, 0.f };
		const Rgba kExpectedBoth{ 24, 72, 136, 200 };

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Magenta shares no channel value with either expectation below, so "the draw never ran" is a
		// distinguishable answer rather than one that could be read as a wrong set.
		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		// Two layouts, one binding each, and deliberately *different* binding numbers: 0 in the first
		// set and 1 in the second. Neither is 15, which SFE_PUSH_BLOCK reserves on OpenGL, and neither
		// layout mixes an image with a buffer, which is what makes Vulkan split one RHI set across two
		// VkDescriptorSetLayouts and shift every later set index.
		DescriptorSetLayoutDesc setZeroDesc;
		setZeroDesc.bindings.push_back({ 0, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto setZeroLayout = rhi().createDescriptorSetLayout(setZeroDesc);
		ASSERT_TRUE(setZeroLayout);
		DescriptorSetLayoutDesc setOneDesc;
		setOneDesc.bindings.push_back({ 1, DescriptorType::UniformBuffer, ShaderStage::Fragment });
		const auto setOneLayout = rhi().createDescriptorSetLayout(setOneDesc);
		ASSERT_TRUE(setOneLayout);

		const auto setZero = rhi().allocateDescriptorSet(setZeroLayout);
		const auto setOne = rhi().allocateDescriptorSet(setOneLayout);
		ASSERT_TRUE(setZero);
		ASSERT_TRUE(setOne);

		const auto bufferZero = makeUniformBuffer(rhi(), kSetZeroColor);
		const auto bufferOne = makeUniformBuffer(rhi(), kSetOneColor);
		ASSERT_TRUE(bufferZero);
		ASSERT_TRUE(bufferOne);

		WriteDescriptor writeZero{};
		writeZero.set = setZero;
		writeZero.binding = 0;
		writeZero.buffers.push_back({ bufferZero, 0, 0 });
		WriteDescriptor writeOne{};
		writeOne.set = setOne;
		writeOne.binding = 1;
		writeOne.buffers.push_back({ bufferOne, 0, 0 });
		rhi().updateDescriptorSets({ writeZero, writeOne });

		const auto expectEveryPixel = [&](const std::vector<uint8_t>& pixels, const Rgba& expected,
			const char* what) {
			ASSERT_FALSE(pixels.empty()) << "readback failed";
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected)
						<< "pixel (" << x << ", " << y << "): " << what;
				}
			}
		};

		// The order of this vector is the set index on Vulkan: element 0 becomes set 0 in the pipeline
		// layout and element 1 becomes set 1, which is what makes the shader's SFE_SET(0, 0) and
		// SFE_SET(1, 1) resolvable. On OpenGL the vector is ignored entirely.
		const auto bothPipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_two_sets.fs",
			{ setZeroLayout, setOneLayout });
		ASSERT_TRUE(bothPipeline) << "pipeline creation failed";

		// Both sets in one call, contiguously from firstSet 0 — the shape LightingPass and Batcher
		// already use in the engine, and the one nothing has ever verified.
		const DescriptorSetHandle bothSets[2] = { setZero, setOne };
		expectEveryPixel(drawWithSets(rhi(), target, pass, bothPipeline, bothSets, 2), kExpectedBoth,
			"the two sets bound in one call did not both reach the fragment shader: red and green come "
			"from the first set, blue and alpha from the second, so a zero pair names which set was "
			"dropped and a swapped pair means the two sets landed on each other's bindings");

		// firstSet, on its own. One set is bound, and it is bound at index 1 rather than 0, against a
		// pipeline layout that still declares both — so the offset is the only thing that can put the
		// buffer where the shader looks for it.
		//
		// The two backends pass this for different reasons, and it is worth being plain about that: on
		// Vulkan firstSet = 1 really does place the set at index 1, which is where the shader's
		// SFE_SET(1, 1) reads. On OpenGL firstSet is discarded along with the set index, and the draw
		// only works because the set's binding number is 1 and SFE_SET collapsed the shader's address
		// to that same 1.
		//
		// So this expectation catches the bind being dropped — the fresh colour below is what makes
		// that failure reachable — but it does not catch firstSet being ignored, and nothing a pixel
		// can say would. Passing 0 here instead of 1 was tried: OpenGL is unchanged, because it never
		// read the offset, and Vulkan draws the right colour too while the validation layer rejects the
		// frame ("statically uses descriptor set 1, but ... the set (1) is out of bounds for the number
		// of sets bound (1)", VUID-vkCmdDraw-None-08600). Reading an unbound set is undefined, and on
		// this driver the undefined answer happened to be the right one. The validation log, not the
		// attachment, is where a wrong firstSet shows up on Vulkan.
		const auto lateSet = rhi().allocateDescriptorSet(setOneLayout);
		ASSERT_TRUE(lateSet);
		// A colour of its own, sharing no channel with the first draw's. Frame one left OpenGL's UBO
		// binding point 1 pointing at bufferOne, so reusing that colour here would let a bind that was
		// silently dropped pass on the leftover state instead of failing.
		const Math::Vec4 kLateColor{ k255(200), k255(16), k255(64), k255(208) };
		const Rgba kExpectedLate{ 200, 16, 64, 208 };
		const auto lateBuffer = makeUniformBuffer(rhi(), kLateColor);
		ASSERT_TRUE(lateBuffer);
		WriteDescriptor writeLate{};
		writeLate.set = lateSet;
		writeLate.binding = 1;
		writeLate.buffers.push_back({ lateBuffer, 0, 0 });
		rhi().updateDescriptorSets({ writeLate });

		// The same two-layout pipeline layout, but a shader that reads only set 1, so set 0 going
		// unbound is not a descriptor the draw statically uses — which is what keeps this frame valid
		// on Vulkan rather than faulting on a set the pipeline declares and nothing filled.
		const auto lateOnlyPipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_set_one.fs",
			{ setZeroLayout, setOneLayout });
		ASSERT_TRUE(lateOnlyPipeline) << "pipeline creation failed";

		expectEveryPixel(drawWithSets(rhi(), target, pass, lateOnlyPipeline, &lateSet, 1, 1), kExpectedLate,
			"a single set bound at firstSet = 1 did not reach the shader's set 1: the offset was not "
			"applied, or the previous frame's binding was still in place");

		// The case this test deliberately does not assert, because the backends disagree on it.
		//
		// Two sets that each declare binding 0 is legal and ordinary on Vulkan — a per-frame set and a
		// per-material set both starting at 0 is the normal way to write one — and it is exactly what
		// OpenGL cannot represent. Probed with the same machinery as above, with each set holding a
		// distinct colour:
		//
		//   Vulkan   (24, 72, 136, 200)   both sets arrived, each at its own set index
		//   OpenGL   (136, 200, 136, 200) the second set's buffer answered *both* reads
		//
		// Nothing warned. On OpenGL the two sets flatten into one list, both entries carry binding 0,
		// and the second glBindBufferBase overwrites the first; the shader side collapses the same way,
		// because SFE_SET turns both blocks into layout(binding = 0) and GLSL is happy to point two
		// uniform blocks at one binding point. So the loss happens twice over, silently, and the frame
		// merely looks wrong.
		//
		// There is therefore no test here for it: a test asserting the Vulkan answer would fail on
		// OpenGL, and one written to accept both would assert nothing. Until the RHI decides what a set
		// index means on OpenGL — offset each set's bindings by a per-set stride, or state that set
		// indices are Vulkan-only and binding numbers must be unique across every set a pipeline binds
		// — the rule callers must follow is the one this test pins: distinct binding numbers per set.
		rhi().destroyGraphicsPipeline(lateOnlyPipeline);
		rhi().destroyGraphicsPipeline(bothPipeline);
		rhi().destroyBuffer(lateBuffer);
		rhi().destroyBuffer(bufferOne);
		rhi().destroyBuffer(bufferZero);
		rhi().freeDescriptorSet(lateSet);
		rhi().freeDescriptorSet(setOne);
		rhi().freeDescriptorSet(setZero);
		rhi().destroyDescriptorSetLayout(setOneLayout);
		rhi().destroyDescriptorSetLayout(setZeroLayout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// Several engine passes swap a texture per draw by rewriting one set between binds inside a single
	// command list. On Vulkan a write to a bound set invalidates the command buffer, so the backend
	// hands the set a fresh copy instead; this pins that a bind keeps the contents it saw, as OpenGL
	// does by capturing the set at bind time.
	TEST_P(RhiBackendTest, DescriptorBindCapturesSetContentsWhenRewrittenInTheSameCommandList) {
		const Rgba kA{ 255, 0, 0, 255 };
		const Rgba kB{ 0, 255, 0, 255 };
		const Rgba kC{ 0, 0, 255, 255 };

		TextureDesc texDesc;
		texDesc.width = 1;
		texDesc.height = 1;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::Sampled | TextureUsage::TransferDst;
		const auto makeSolid = [&](const Rgba& c) {
			const uint8_t bytes[4] = { c.r, c.g, c.b, c.a };
			return rhi().createTexture(texDesc, bytes);
		};
		const auto texA = makeSolid(kA);
		const auto texB = makeSolid(kB);
		const auto texC = makeSolid(kC);
		ASSERT_TRUE(texA && texB && texC);

		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Nearest;
		samplerDesc.magFilter = FilterMode::Nearest;
		const auto sampler = rhi().createSampler(samplerDesc);
		ASSERT_TRUE(sampler);

		const auto pass = makePass(rhi(), { 1.f, 0.f, 1.f, 1.f });
		ASSERT_TRUE(pass);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_sampler.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		TextureHandle targets[3];
		FramebufferHandle framebuffers[3];
		for (int i = 0; i < 3; ++i) {
			targets[i] = makeTarget(rhi());
			ASSERT_TRUE(targets[i]);
			FramebufferDesc fbDesc;
			fbDesc.width = kSize;
			fbDesc.height = kSize;
			fbDesc.layers = 1;
			fbDesc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = targets[i];
			att.layer = UINT32_MAX;
			fbDesc.colorAttachments = { att };
			framebuffers[i] = rhi().createFramebuffer(fbDesc);
			ASSERT_TRUE(framebuffers[i]);
		}

		const auto writeTexture = [&](TextureHandle tex) {
			WriteDescriptor write{};
			write.set = set;
			write.binding = 0;
			write.images.push_back({ tex, sampler });
			rhi().updateDescriptorSets({ write });
		};

		rhi().beginFrame();
		const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
		const auto beginPass = [&](int i) {
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = framebuffers[i];
			begin.width = kSize;
			begin.height = kSize;
			rhi().cmdBeginRenderPass(cmd, begin);
			rhi().cmdBindPipeline(cmd, pipeline);
		};

		writeTexture(texA);
		beginPass(0);
		rhi().cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
		rhi().cmdDraw(cmd, 3, 1, 0, 0);
		rhi().cmdEndRenderPass(cmd);

		writeTexture(texB);
		beginPass(1);
		rhi().cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
		rhi().cmdDraw(cmd, 3, 1, 0, 0);
		rhi().cmdEndRenderPass(cmd);

		// Rewritten but not rebound: the draw runs on whatever the last bind captured.
		writeTexture(texC);
		beginPass(2);
		rhi().cmdDraw(cmd, 3, 1, 0, 0);
		rhi().cmdEndRenderPass(cmd);

		rhi().endCommandList(cmd);
		rhi().submit(cmd);
		rhi().present();
		rhi().waitIdle();

		const Rgba expected[3] = { kA, kB, kB };
		for (int i = 0; i < 3; ++i) {
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			ASSERT_TRUE(rhi().readbackTexture(targets[i], pixels, w, h)) << "readback of target " << i << " failed";
			for (int y = 0; y < kSize; ++y) {
				for (int x = 0; x < kSize; ++x) {
					EXPECT_EQ(pixelAt(pixels, x, y), expected[i])
						<< "target " << i << " pixel (" << x << ", " << y << ") does not show the texture "
						   "the set held at its last bind";
				}
			}
		}

		for (int i = 0; i < 3; ++i) {
			rhi().destroyFramebuffer(framebuffers[i]);
			rhi().destroyTexture(targets[i]);
		}
		rhi().destroyGraphicsPipeline(pipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroySampler(sampler);
		rhi().destroyTexture(texC);
		rhi().destroyTexture(texB);
		rhi().destroyTexture(texA);
	}

	// A sampler reads the texture at its declared binding, whatever the GLSL variable is called.
	// OpenGLRHI::createGraphicsPipeline overrides sampler units by name after linking, from a fixed
	// table written for the engine's own shaders; worldPos is in it with unit 1. The shader declares
	// worldPos at binding 2 and a second table name at binding 1, so a backend that follows the table
	// samples the red texture and one that follows the binding samples the blue one.
	TEST_P(RhiBackendTest, DescriptorSamplerUnitFollowsBindingNotName) {
		const Rgba kBlue{ 0, 0, 255, 255 };

		const auto makeSolidTexture = [&](uint8_t r, uint8_t g, uint8_t b) {
			const std::vector<uint8_t> texels = {
				r, g, b, 255,   r, g, b, 255,
				r, g, b, 255,   r, g, b, 255,
			};
			TextureDesc texDesc;
			texDesc.width = 2;
			texDesc.height = 2;
			texDesc.depth = 1;
			texDesc.arrayLayers = 1;
			texDesc.dimension = TextureDimension::Tex2D;
			texDesc.format = TextureFormat::RGBA8;
			texDesc.usage = TextureUsage::Sampled | TextureUsage::TransferDst;
			return rhi().createTexture(texDesc, texels.data());
		};
		const auto red = makeSolidTexture(255, 0, 0);
		const auto blue = makeSolidTexture(0, 0, 255);
		ASSERT_TRUE(red);
		ASSERT_TRUE(blue);

		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Nearest;
		samplerDesc.magFilter = FilterMode::Nearest;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		const auto sampler = rhi().createSampler(samplerDesc);
		ASSERT_TRUE(sampler);

		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		// Green: neither texture's colour, so a draw that never ran is its own failure.
		const auto pass = makePass(rhi(), { 0.f, 1.f, 0.f, 1.f });
		ASSERT_TRUE(pass);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.push_back({ 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		layoutDesc.bindings.push_back({ 2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment });
		const auto layout = rhi().createDescriptorSetLayout(layoutDesc);
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		WriteDescriptor atOne{};
		atOne.set = set;
		atOne.binding = 1;
		atOne.images.push_back({ red, sampler });
		WriteDescriptor atTwo{};
		atTwo.set = set;
		atTwo.binding = 2;
		atTwo.images.push_back({ blue, sampler });
		rhi().updateDescriptorSets({ atOne, atTwo });

		const auto pipeline = makeDescriptorPipeline(rhi(), pass, "shaders/rhi_test_sampler_names.fs", { layout });
		ASSERT_TRUE(pipeline) << "pipeline creation failed";

		const auto pixels = drawWithSets(rhi(), target, pass, pipeline, &set, 1);
		ASSERT_FALSE(pixels.empty()) << "readback failed";

		const Rgba got = pixelAt(pixels, 0, 0);
		std::cout << "[ INFO     ] worldPos at binding 2 sampled RGBA(" << static_cast<int>(got.r) << ", "
			<< static_cast<int>(got.g) << ", " << static_cast<int>(got.b) << ", " << static_cast<int>(got.a)
			<< ")\n";
		for (int y = 0; y < kSize; ++y) {
			for (int x = 0; x < kSize; ++x) {
				EXPECT_EQ(pixelAt(pixels, x, y), kBlue)
					<< "pixel (" << x << ", " << y << ") is not the texture bound at worldPos's declared "
					   "binding 2: red means the sampler unit was reassigned by the variable's name";
			}
		}

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
		rhi().destroySampler(sampler);
		rhi().destroyTexture(blue);
		rhi().destroyTexture(red);
	}
}
