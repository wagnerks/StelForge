#include "RhiTestFixture.h"

// Compute. Every other file in this suite drives the graphics pipeline; nothing here has ever
// dispatched a shader, so createComputePipeline and cmdDispatch are two interface entry points
// whose only evidence of working is that the engine's skinning pass does not visibly misbehave.
//
// Two things had to be established before the first test could be written, and both are worth
// recording here because they shape everything below.
//
// 1. Which command list. beginCommandList(CommandListType::Compute) is a separate checkbox, so
//    these tests use Graphics, and it works on both backends: OpenGL ignores the type entirely and
//    replays whatever was recorded, and Vulkan hands every list to the same graphics queue, which
//    on this device also carries VK_QUEUE_COMPUTE_BIT. A dispatch is recorded outside any render
//    pass, which is what Vulkan requires of vkCmdDispatch.
//
// 2. How the result is read back. The plan's wording for this checkbox is "read back through
//    mapBuffer", and that turns out not to be expressible: no single CpuAccess value gives a
//    readable mapping of a storage buffer on both backends. Measured, on a Storage buffer, with
//    mapBuffer(MapMode::Read):
//
//                          OpenGL     Vulkan
//        CpuAccess::None     ptr      nullptr
//        CpuAccess::Read     ptr      nullptr
//        CpuAccess::Write   nullptr     ptr
//        CpuAccess::ReadWrite nullptr   ptr
//
//    The two backends are exact opposites. OpenGL's toGLMapFlags adds GL_MAP_INVALIDATE_RANGE_BIT
//    and GL_MAP_UNSYNCHRONIZED_BIT whenever cpuAccess is Write or ReadWrite, and GL makes it an
//    error to combine either with GL_MAP_READ_BIT, so glMapBufferRange fails; Vulkan's createBuffer
//    treats only Write and ReadWrite as host-mapped and puts Read and None in device-local memory
//    that vmaMapMemory cannot map. So the one enumerator that says "GPU to CPU" is the one Vulkan
//    cannot read, and the one Vulkan can read is the one OpenGL refuses. Picking either would need
//    a per-backend branch, which this suite does not do.
//
//    The result is therefore staged out through a fragment shader instead: the same storage buffer
//    is bound to a full-screen draw that emits each element's four bytes as one RGBA8 pixel, and
//    readbackTexture brings them back. That path is not taken on trust -- it is exactly what
//    DescriptorStorageBufferIsIndexableFromTheFragmentShader already pins on both backends, so a
//    failure here is about the dispatch rather than about the readback. The uint32 the host
//    reassembles is the exact uint32 the compute shader wrote.
namespace SFE::Render::Tests {
	namespace {

		// Must match layout(local_size_x = 4) in rhi_test_compute.cs. Four rather than one so that
		// the dispatch covers more elements than it has groups, and a backend that confused groups
		// with invocations writes a quarter of the range and fails.
		constexpr uint32_t kLocalSizeX = 4;
		constexpr uint32_t kGroupCount = 4;
		constexpr uint32_t kWritten = kLocalSizeX * kGroupCount;

		// Four slots past the end of the dispatch. They must still hold the sentinel afterwards,
		// which is what separates "sixteen invocations each wrote their own slot" from "something
		// scribbled over the whole buffer".
		constexpr uint32_t kCount = kWritten + 4;

		// Nothing the shader can produce: its values all carry the 0xA5A5 marker in their high half.
		constexpr uint32_t kSentinel = 0xDEADBEEFu;

		// The same expression as rhi_test_compute.cs, kept here so the two can be read side by side.
		constexpr uint32_t expectedValue(uint32_t i) { return 0xA5A50000u | (1000u + i * 7u); }

		BufferHandle makeStorageBuffer(RenderHardwareInterface& rhi, const std::vector<uint32_t>& initial) {
			BufferDesc desc;
			desc.size = initial.size() * sizeof(uint32_t);
			desc.usage = BufferUsage::Storage;
			// Write, not Read: this is the value the engine's own compute pass uses for its storage
			// buffers, and it is the only one that leaves the buffer in ordinary GPU memory on both
			// backends. Nothing here maps the buffer, so the mapping inversion above does not bite.
			desc.cpuAccess = CpuAccess::Write;
			desc.debugName = "rhi_test_compute_storage";
			return rhi.createBuffer(desc, initial.data());
		}

		// One binding, visible to both stages, so a single set can be bound to the compute pipeline
		// that writes the buffer and to the graphics pipeline that reads it back. Binding 0, not 15,
		// which SFE_PUSH_BLOCK reserves on OpenGL, and one set at index 0, which is all OpenGL can
		// express since it discards the set index.
		DescriptorSetLayoutHandle makeStorageLayout(RenderHardwareInterface& rhi) {
			DescriptorSetLayoutDesc desc;
			desc.bindings.push_back({ 0, DescriptorType::StorageBuffer,
				ShaderStage::Compute | ShaderStage::Fragment });
			return rhi.createDescriptorSetLayout(desc);
		}

		PipelineHandle makeComputePipeline(RenderHardwareInterface& rhi, DescriptorSetLayoutHandle layout,
			const std::string& shader) {
			ComputePipelineDesc desc;
			desc.compute = rhi.loadShader(ShaderStage::Compute, shader);
			// The layout goes into the pipeline as well as into the set: on Vulkan it is what the
			// pipeline layout is built from, and dispatching with a pipeline whose layout declares a
			// set that was never bound faults.
			desc.descriptorSetLayouts = { layout };
			return rhi.createComputePipeline(desc);
		}

		// One dispatch, in its own frame. Group counts are parameters rather than constants so that
		// the group-count case can reuse this unchanged.
		void dispatch(RenderHardwareInterface& rhi, PipelineHandle pipeline, DescriptorSetHandle set,
			uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			rhi.cmdBindPipeline(cmd, pipeline);
			// After the pipeline: Vulkan resolves the pipeline layout out of the handle passed here,
			// and OpenGL's own bind paths already require that order.
			rhi.cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
			rhi.cmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
			// Not decoration, and not the barrier checkbox either -- this is the memory dependency
			// both APIs require between a shader writing a storage buffer and anything reading it,
			// and it is the same barrier AnimationComputePass emits after its own dispatch. Neither
			// backend gets it for free from the frame boundary: OpenGL waitIdle is glFinish, which
			// orders commands but does not make incoherent shader stores visible to a later fetch,
			// and Vulkan submit carries no implicit dependency between a dispatch and a later draw.
			// Measured: this test still passes with these five lines deleted, on both backends and
			// over repeated runs, which is exactly why they are here rather than left to a driver
			// being generous.
			rhi.cmdPipelineBarrier(cmd, {
				PipelineStage::Compute,
				PipelineStage::FragmentShader,
				AccessMask::ShaderWrite,
				AccessMask::ShaderRead,
			});
			rhi.endCommandList(cmd);
			// Recording is not execution: OpenGL replays the recorded list here and Vulkan hands it
			// to the queue.
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		// Reads `count` uint32 elements of the storage buffer behind `set` back to the host, by
		// drawing them into a count x 1 RGBA8 attachment one element per pixel and reading that
		// back. See the file header for why this is not mapBuffer.
		std::vector<uint32_t> readStorageBuffer(RenderHardwareInterface& rhi,
			DescriptorSetLayoutHandle layout, DescriptorSetHandle set, uint32_t count) {
			TextureDesc texDesc;
			texDesc.width = count;
			texDesc.height = 1;
			texDesc.depth = 1;
			texDesc.arrayLayers = 1;
			texDesc.dimension = TextureDimension::Tex2D;
			texDesc.format = TextureFormat::RGBA8;
			texDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
			const auto target = rhi.createTexture(texDesc);

			RenderPassDesc passDesc;
			passDesc.colorAttachmentCount = 1;
			passDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
			passDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
			passDesc.colorAttachments[0].format = TextureFormat::RGBA8;
			// Cleared to zero, which is a value the shader cannot write -- every element it emits
			// carries the 0xA5A5 marker -- so a draw that never covered a pixel reads back as 0 and
			// is told apart from an element that came back wrong.
			passDesc.colorAttachments[0].clearColor = { 0.f, 0.f, 0.f, 0.f };
			const auto pass = rhi.createRenderPass(passDesc);

			GraphicsPipelineDesc pipeDesc;
			pipeDesc.renderPass = pass;
			pipeDesc.topology = PrimitiveTopology::Triangles;
			pipeDesc.cullMode = CullMode::None;
			pipeDesc.frontFace = FrontFace::CounterClockWise;
			pipeDesc.fillMode = PolygonFillMode::Fill;
			pipeDesc.depthStencilState = { false, false, CompareOp::Always };
			// Blending off, so the four bytes reach the attachment unmodified -- an alpha of 0xA5
			// would otherwise be a weight rather than a value.
			pipeDesc.blend = { false };
			pipeDesc.descriptorSetLayouts = { layout };
			pipeDesc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_fullscreen.vs");
			pipeDesc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_compute_readback.fs");
			const auto pipeline = rhi.createGraphicsPipeline(pipeDesc);

			FramebufferDesc fbDesc;
			fbDesc.width = count;
			fbDesc.height = 1;
			fbDesc.layers = 1;
			fbDesc.renderPass = pass;
			FramebufferAttachmentDesc att;
			att.texture = target;
			att.layer = UINT32_MAX;
			fbDesc.colorAttachments = { att };
			const auto fb = rhi.createFramebuffer(fbDesc);

			std::vector<uint32_t> values;
			if (target && pass && pipeline && fb) {
				rhi.beginFrame();
				const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
				RenderPassBeginInfo begin;
				begin.renderPass = pass;
				begin.framebuffer = fb;
				begin.width = count;
				begin.height = 1;
				rhi.cmdBeginRenderPass(cmd, begin);
				rhi.cmdBindPipeline(cmd, pipeline);
				rhi.cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
				rhi.cmdDraw(cmd, 3, 1, 0, 0);
				rhi.cmdEndRenderPass(cmd);
				rhi.endCommandList(cmd);
				rhi.submit(cmd);
				rhi.present();
				rhi.waitIdle();

				std::vector<uint8_t> pixels;
				int w = 0;
				int h = 0;
				if (rhi.readbackTexture(target, pixels, w, h) && pixels.size() >= count * 4u) {
					values.resize(count);
					for (uint32_t i = 0; i < count; ++i) {
						const uint8_t* p = pixels.data() + i * 4u;
						values[i] = static_cast<uint32_t>(p[0])
							| (static_cast<uint32_t>(p[1]) << 8)
							| (static_cast<uint32_t>(p[2]) << 16)
							| (static_cast<uint32_t>(p[3]) << 24);
					}
				}
			}

			rhi.destroyFramebuffer(fb);
			rhi.destroyGraphicsPipeline(pipeline);
			rhi.destroyRenderPass(pass);
			rhi.destroyTexture(target);
			return values;
		}
	}

	// The whole compute path in one dispatch: a compute shader built by createComputePipeline, a
	// storage buffer described by a layout and written into a set, a cmdDispatch, and the bytes read
	// back on the host. Every invocation writes a value derived from its own gl_GlobalInvocationID,
	// so this fails rather than passes if the shader ran once, if every invocation saw the same id,
	// or if the four groups turned into one.
	TEST_P(RhiBackendTest, ComputeShaderFillsAStorageBufferOneSlotPerInvocation) {
		std::vector<uint32_t> initial(kCount, kSentinel);

		const auto buffer = makeStorageBuffer(rhi(), initial);
		ASSERT_TRUE(buffer);

		const auto layout = makeStorageLayout(rhi());
		ASSERT_TRUE(layout);

		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);

		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		// range 0 means the whole buffer, which is what an unsized array in the shader block expects.
		write.buffers.push_back({ buffer, 0, 0 });
		rhi().updateDescriptorSets({ write });

		const auto pipeline = makeComputePipeline(rhi(), layout, "shaders/rhi_test_compute.cs");
		ASSERT_TRUE(pipeline) << "compute pipeline creation failed";

		// The buffer must start at the sentinel, or the assertions below could be satisfied by a
		// dispatch that never happened at all. This also proves the readback path itself works
		// before the dispatch is asked to prove anything.
		const auto before = readStorageBuffer(rhi(), layout, set, kCount);
		ASSERT_EQ(before.size(), kCount) << "readback failed";
		for (uint32_t i = 0; i < kCount; ++i) {
			ASSERT_EQ(before[i], kSentinel)
				<< "element " << i << " did not start at the sentinel, so nothing below would be "
				   "evidence that the dispatch wrote it";
		}

		dispatch(rhi(), pipeline, set, kGroupCount);

		const auto after = readStorageBuffer(rhi(), layout, set, kCount);
		ASSERT_EQ(after.size(), kCount) << "readback failed";

		// Each of the sixteen dispatched invocations wrote its own slot, and wrote its own value:
		// 0xA5A503E8, 0xA5A503EF, 0xA5A503F6 ... 0xA5A50451. All sixteen differ, so a shader that
		// wrote one slot repeatedly, or wrote the right count of slots with the wrong index, is a
		// different failure from a shader that did not run.
		for (uint32_t i = 0; i < kWritten; ++i) {
			EXPECT_EQ(after[i], expectedValue(i))
				<< "element " << i << " is 0x" << std::hex << after[i] << ", expected 0x"
				<< expectedValue(i) << std::dec << ": the compute shader did not write this slot, or "
				   "the invocation that owns it did not see gl_GlobalInvocationID.x == " << i;
		}

		// And nothing past the dispatch moved. Four groups of four is sixteen invocations, not
		// twenty, so a backend that scaled the group count by the local size, or that ran one
		// invocation per element regardless of what was asked, overruns into these and fails here.
		for (uint32_t i = kWritten; i < kCount; ++i) {
			EXPECT_EQ(after[i], kSentinel)
				<< "element " << i << " is 0x" << std::hex << after[i] << std::dec
				<< " but lies past the " << kWritten << " invocations dispatched: the dispatch wrote "
				   "outside the range " << kGroupCount << " groups of " << kLocalSizeX << " covers";
		}

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyBuffer(buffer);
	}

	// Three claims about cmdCopyBuffer in one pass: it lands at the destination offset it was given, it
	// reads from the source offset it was given, and it leaves the rest of the destination alone. The two
	// offsets differ from each other and from zero, so a backend that ignored either would be caught.
	//
	// Nothing here writes the source later in the same list. When a recorded update lands is a separate
	// contract the backends do not share -- see RhiFrameTests -- and the frame that copies its transforms
	// into last frame's slot does it after everything that reads them, so it never depends on that.
	TEST_P(RhiBackendTest, CopyBufferMovesTheRangeItWasGivenAndNothingElse) {
		constexpr uint32_t kCopyFrom = 4;   // in elements
		constexpr uint32_t kCopyTo = 8;
		constexpr uint32_t kCopyCount = 4;
		constexpr uint32_t kDestFill = 0xEEEEEEEEu;

		std::vector<uint32_t> sourceData(kCount);
		for (uint32_t i = 0; i < kCount; ++i) {
			sourceData[i] = 0xC0DE0000u + i;
		}
		const auto source = makeStorageBuffer(rhi(), sourceData);
		const auto destination = makeStorageBuffer(rhi(), std::vector<uint32_t>(kCount, kDestFill));
		ASSERT_TRUE(source);
		ASSERT_TRUE(destination);

		const auto layout = makeStorageLayout(rhi());
		ASSERT_TRUE(layout);
		const auto set = rhi().allocateDescriptorSet(layout);
		ASSERT_TRUE(set);
		WriteDescriptor write{};
		write.set = set;
		write.binding = 0;
		write.buffers.push_back({ destination, 0, 0 });
		rhi().updateDescriptorSets({ write });

		rhi().beginFrame();
		const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
		rhi().cmdCopyBuffer(cmd, source, destination, kCopyFrom * sizeof(uint32_t), kCopyTo * sizeof(uint32_t),
			kCopyCount * sizeof(uint32_t));
		rhi().cmdPipelineBarrier(cmd, {
			PipelineStage::Transfer,
			PipelineStage::FragmentShader,
			AccessMask::TransferWrite,
			AccessMask::ShaderRead,
		});
		rhi().endCommandList(cmd);
		rhi().submit(cmd);
		rhi().present();
		rhi().waitIdle();

		const auto read = readStorageBuffer(rhi(), layout, set, kCount);
		ASSERT_EQ(read.size(), kCount) << "readback failed";
		for (uint32_t i = 0; i < kCount; ++i) {
			const bool copied = i >= kCopyTo && i < kCopyTo + kCopyCount;
			const uint32_t expected = copied ? sourceData[kCopyFrom + (i - kCopyTo)] : kDestFill;
			EXPECT_EQ(read[i], expected) << "element " << i << (copied
				? " is not the source element the offsets name"
				: " was touched, and it is outside the copied range");
		}

		rhi().freeDescriptorSet(set);
		rhi().destroyDescriptorSetLayout(layout);
		rhi().destroyBuffer(source);
		rhi().destroyBuffer(destination);
	}
}
