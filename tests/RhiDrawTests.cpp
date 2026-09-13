#include "RhiTestFixture.h"

// Geometry input: where a draw takes its vertices from. Everything else in the suite so far draws
// from the vertex index alone, which never touches VertexLayoutDesc, cmdBindVertexBuffer or the
// backends' very different ideas of a vertex binding — OpenGL programs the pipeline's VAO at bind
// time, Vulkan records a vkCmdBindVertexBuffers. Both have to put the triangle in the same pixels.
namespace SFE::Render::Tests {
	namespace {

		constexpr int kSize = 8;

		// Position and colour in one buffer, so the colour attribute sits at a non-zero offset and
		// the stride has to be honoured for either of them to arrive.
		struct Vertex {
			float x = 0.f;
			float y = 0.f;
			float r = 0.f;
			float g = 0.f;
			float b = 0.f;
			float a = 1.f;
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

		PipelineHandle makeVertexBufferPipeline(RenderHardwareInterface& rhi, RenderPassHandle pass) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			// Winding is pinned by RhiConventionTests; culling here would only let a winding bug
			// masquerade as a vertex buffer bug.
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.layout.bindings.push_back({ 0, sizeof(Vertex), VertexInputRate::PerVertex });
			desc.layout.attributes.push_back({ 0, 0, VertexFormat::Float2, offsetof(Vertex, x), false });
			desc.layout.attributes.push_back({ 1, 0, VertexFormat::Float4, offsetof(Vertex, r), false });
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_vertex.vs");
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_vertex.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		FramebufferHandle makeTargetFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass, TextureHandle target) {
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

		// One frame, one draw of three vertices taken from byteOffset bytes into the buffer. The two
		// draws under test differ in that one number and nothing else, so anything the frame does
		// besides honour the offset is shared and cannot explain a difference in the result.
		void drawThreeVerticesFrom(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, BufferHandle vbo, uint32_t byteOffset) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			// Pipeline first: the OpenGL backend resolves the binding's stride out of the currently
			// bound pipeline's layout, so a vertex buffer bound ahead of one is dropped on the floor.
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdBindVertexBuffer(cmd, 0, vbo, byteOffset);
			rhi.cmdDraw(cmd, 3, 1, 0, 0);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		// One frame, one indexed draw of three indices. Everything except the index type and the two
		// arguments under test is shared with the other draws in the indexed test, so a difference in
		// the picture can only come from the numbers passed here.
		void drawIndexedTriangle(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, BufferHandle vbo, BufferHandle ibo, IndexType type,
			uint32_t firstIndex, int32_t vertexOffset) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			// Pipeline first for both binds: the OpenGL backend hangs the index buffer off the bound
			// pipeline's VAO the same way it resolves a vertex binding's stride, so either bind
			// recorded ahead of a pipeline is dropped without a word.
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdBindVertexBuffer(cmd, 0, vbo, 0);
			rhi.cmdBindIndexBuffer(cmd, ibo, type, 0);
			rhi.cmdDrawIndexed(cmd, 3, 1, firstIndex, vertexOffset, 0);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		// One per-instance record: where this copy of the triangle goes and what colour it takes.
		// Same shape as Vertex on purpose, so the only thing telling the two buffers apart is the
		// input rate declared on their bindings.
		struct Instance {
			float ox = 0.f;
			float oy = 0.f;
			float r = 0.f;
			float g = 0.f;
			float b = 0.f;
			float a = 1.f;
		};

		// Binding 0 per vertex, binding 1 per instance. Two bindings rather than one is the point:
		// a single buffer could not show a divisor at all, since every rate would look the same.
		PipelineHandle makeInstancedPipeline(RenderHardwareInterface& rhi, RenderPassHandle pass) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = { false };
			desc.layout.bindings.push_back({ 0, sizeof(Vertex), VertexInputRate::PerVertex });
			desc.layout.bindings.push_back({ 1, sizeof(Instance), VertexInputRate::PerInstance });
			desc.layout.attributes.push_back({ 0, 0, VertexFormat::Float2, offsetof(Vertex, x), false });
			desc.layout.attributes.push_back({ 1, 0, VertexFormat::Float4, offsetof(Vertex, r), false });
			desc.layout.attributes.push_back({ 2, 1, VertexFormat::Float2, offsetof(Instance, ox), false });
			desc.layout.attributes.push_back({ 3, 1, VertexFormat::Float4, offsetof(Instance, r), false });
			desc.shaders.vertex = rhi.loadShader(ShaderStage::Vertex, "shaders/rhi_test_instanced.vs");
			// The fragment stage only writes the flat colour it is handed, which is exactly what the
			// non-instanced tests need too, so it is shared rather than copied.
			desc.shaders.fragment = rhi.loadShader(ShaderStage::Fragment, "shaders/rhi_test_vertex.fs");
			return rhi.createGraphicsPipeline(desc);
		}

		// One frame, one instanced draw of the same three vertices. The two draws under test differ in
		// instanceCount and firstInstance and in nothing else, so a difference in the picture can only
		// come from those two numbers.
		void drawInstances(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, BufferHandle vbo, BufferHandle instances,
			uint32_t instanceCount, uint32_t firstInstance) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			// Pipeline first for both binds: the OpenGL backend resolves each binding's stride and
			// divisor out of the currently bound pipeline's layout, so a vertex buffer bound ahead of
			// one is dropped on the floor.
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdBindVertexBuffer(cmd, 0, vbo, 0);
			rhi.cmdBindVertexBuffer(cmd, 1, instances, 0);
			rhi.cmdDraw(cmd, 3, instanceCount, 0, firstInstance);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		// One draw-argument record, as both backends read it off the GPU: four uint32_t in the order
		// vertexCount, instanceCount, firstVertex, firstInstance. Same bytes as VkDrawIndirectCommand
		// and OpenGL's DrawArraysIndirectCommand, which is what cmdDrawIndirect's default stride of 16
		// already assumes; a record laid out any other way would make the default stride a lie.
		struct DrawArgs {
			uint32_t vertexCount = 0;
			uint32_t instanceCount = 0;
			uint32_t firstVertex = 0;
			uint32_t firstInstance = 0;
		};
		static_assert(sizeof(DrawArgs) == 16, "the interface's default indirect stride is 16 bytes");

		// One frame, one indirect draw. The draws under test differ in offset and drawCount and in
		// nothing else — same buffer, same pipeline, same vertices — so a difference in the picture can
		// only come from which records were read. The stride is left at the interface's default rather
		// than passed, so the default itself is under test.
		void drawIndirectFrom(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, BufferHandle vbo, BufferHandle args, size_t offset, uint32_t drawCount) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			// Pipeline first: the OpenGL backend resolves the binding's stride out of the currently
			// bound pipeline's layout, so a vertex buffer bound ahead of one is dropped on the floor.
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdBindVertexBuffer(cmd, 0, vbo, 0);
			rhi.cmdDrawIndirect(cmd, args, offset, drawCount);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		// One indexed draw-argument record, as both backends read it off the GPU: five words in the
		// order indexCount, instanceCount, firstIndex, vertexOffset, firstInstance. Same bytes as
		// VkDrawIndexedIndirectCommand and OpenGL's DrawElementsIndirectCommand, which is why
		// cmdDrawIndexedIndirect's default stride is 20 where cmdDrawIndirect's is 16 — the indexed
		// record carries one extra field. vertexOffset is declared signed in both specs, so it is
		// signed here too rather than smuggled in as a uint32_t.
		struct DrawIndexedArgs {
			uint32_t indexCount = 0;
			uint32_t instanceCount = 0;
			uint32_t firstIndex = 0;
			int32_t vertexOffset = 0;
			uint32_t firstInstance = 0;
		};
		static_assert(sizeof(DrawIndexedArgs) == 20, "the interface's default indexed indirect stride is 20 bytes");

		// One frame, one indexed indirect draw. The draws under test differ in offset and drawCount and
		// in nothing else — same buffer, same indices, same vertices — so a difference in the picture can
		// only come from which records were read. The stride is left at the interface's default rather
		// than passed, so the default itself is under test.
		void drawIndexedIndirectFrom(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, BufferHandle vbo, BufferHandle ibo, BufferHandle args, size_t offset, uint32_t drawCount) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			// Pipeline first for both binds: the OpenGL backend hangs the index buffer off the bound
			// pipeline's VAO and resolves the vertex binding's stride out of its layout, so either bind
			// recorded ahead of a pipeline is dropped without a word.
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdBindVertexBuffer(cmd, 0, vbo, 0);
			rhi.cmdBindIndexBuffer(cmd, ibo, IndexType::Uint16, 0);
			rhi.cmdDrawIndexedIndirect(cmd, args, offset, drawCount);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		// One frame, one indirect draw against the instanced pipeline: the same records as
		// drawIndirectFrom reads, but with a per-instance binding in place so the instanceCount and
		// firstInstance words inside a record finally have somewhere to show. Everything except
		// offset and drawCount is shared between the draws under test, so a difference in the
		// picture can only come from which records were read and what they said.
		void drawInstancedIndirectFrom(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb,
			PipelineHandle pipeline, BufferHandle vbo, BufferHandle instances, BufferHandle args,
			size_t offset, uint32_t drawCount) {
			rhi.beginFrame();
			const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kSize;
			begin.height = kSize;
			rhi.cmdBeginRenderPass(cmd, begin);
			// Pipeline first for both binds: the OpenGL backend resolves each binding's stride and
			// divisor out of the currently bound pipeline's layout, so a vertex buffer bound ahead of
			// one is dropped on the floor.
			rhi.cmdBindPipeline(cmd, pipeline);
			rhi.cmdBindVertexBuffer(cmd, 0, vbo, 0);
			rhi.cmdBindVertexBuffer(cmd, 1, instances, 0);
			rhi.cmdDrawIndirect(cmd, args, offset, drawCount);
			rhi.cmdEndRenderPass(cmd);
			rhi.endCommandList(cmd);
			rhi.submit(cmd);
			rhi.present();
			rhi.waitIdle();
		}

		Rgba pixelAt(const std::vector<uint8_t>& pixels, int x, int y) {
			const size_t offset = (static_cast<size_t>(y) * kSize + static_cast<size_t>(x)) * 4u;
			if (offset + 3 >= pixels.size()) {
				return {};
			}
			return { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
		}
	}

	// A vertex buffer put in front of the pipeline has to decide where the triangle lands. The
	// triangle chosen here covers six pixels of the top row and two of the second and nothing else,
	// a shape that survives neither a flip nor a transpose, so a backend that reads the buffer with
	// the wrong stride, the wrong attribute offset or the right data in the wrong orientation fails
	// on a named pixel instead of drawing something plausible.
	TEST_P(RhiBackendTest, VertexBufferPositionsDecideWhereTheTriangleLands) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeVertexBufferPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a vertex layout failed to build";

		// Clip space, Y up. The slanted edge runs from (-1, 0.5) to (1, 1), which is the line
		// y = 0.25x + 0.75; on an 8x8 target every pixel centre sits a clean half pixel off that
		// edge, so coverage is not a rounding coin flip. Counter-clockwise, though culling is off.
		const Vertex verts[3] = {
			{ -1.f, 1.0f, 0.f, 1.f, 1.f, 1.f },
			{ -1.f, 0.5f, 0.f, 1.f, 1.f, 1.f },
			{  1.f, 1.0f, 0.f, 1.f, 1.f, 1.f },
		};

		BufferDesc bufDesc;
		bufDesc.size = sizeof(verts);
		bufDesc.usage = BufferUsage::Vertex;
		bufDesc.cpuAccess = CpuAccess::Write;
		bufDesc.debugName = "rhi_test_vertex_buffer";
		const auto vbo = rhi().createBuffer(bufDesc, verts);
		ASSERT_TRUE(vbo);

		FramebufferDesc fbDesc;
		fbDesc.width = kSize;
		fbDesc.height = kSize;
		fbDesc.layers = 1;
		fbDesc.renderPass = pass;
		FramebufferAttachmentDesc att;
		att.texture = target;
		att.layer = UINT32_MAX;
		fbDesc.colorAttachments = { att };
		const auto fb = rhi().createFramebuffer(fbDesc);
		ASSERT_TRUE(fb);

		rhi().beginFrame();
		const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
		RenderPassBeginInfo begin;
		begin.renderPass = pass;
		begin.framebuffer = fb;
		begin.width = kSize;
		begin.height = kSize;
		rhi().cmdBeginRenderPass(cmd, begin);
		// Pipeline first: the OpenGL backend resolves the binding's stride out of the currently
		// bound pipeline's layout, so a vertex buffer bound ahead of one is dropped on the floor.
		rhi().cmdBindPipeline(cmd, pipeline);
		rhi().cmdBindVertexBuffer(cmd, 0, vbo, 0);
		rhi().cmdDraw(cmd, 3, 1, 0, 0);
		rhi().cmdEndRenderPass(cmd);
		rhi().endCommandList(cmd);
		rhi().submit(cmd);
		rhi().present();
		rhi().waitIdle();

		std::vector<uint8_t> pixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(target, pixels, w, h));
		ASSERT_FALSE(pixels.empty());

		const Rgba painted{ 0, 255, 255, 255 };
		const Rgba clear{ 0, 0, 0, 255 };
		// Inside the top row's span, and its mirror images are not: (0, 4) is the transpose of this
		// pixel and (4, 7) is its vertical flip, both of which are outside the triangle.
		EXPECT_EQ(pixelAt(pixels, 4, 0), painted) << "the triangle described by the vertex buffer never arrived";
		// Past the slanted edge on the top row: a layout that stretched the positions, or a draw
		// that fell back to a full-screen triangle, would paint here.
		EXPECT_EQ(pixelAt(pixels, 7, 0), clear) << "painted right of the slanted edge";
		// The transpose partner of the painted pixel above.
		EXPECT_EQ(pixelAt(pixels, 0, 4), clear) << "the result is transposed";
	}

	// cmdBindVertexBuffer's offset is a byte offset into the buffer, not a decoration: with six
	// vertices in one buffer, the same three-vertex draw has to pick the first triangle at offset 0
	// and the second one at offset 3 * sizeof(Vertex). The two triangles sit in opposite corners and
	// carry different colours, so an offset that is ignored, applied in vertices instead of bytes or
	// applied to the wrong binding paints the wrong corner or the wrong colour rather than something
	// that could pass by luck. Both backends have to agree on what the number means: OpenGL feeds it
	// to glVertexArrayVertexBuffer, Vulkan to vkCmdBindVertexBuffers.
	TEST_P(RhiBackendTest, VertexBufferOffsetSelectsWhichVerticesAreRead) {
		const auto first = makeTarget(rhi());
		const auto second = makeTarget(rhi());
		ASSERT_TRUE(first);
		ASSERT_TRUE(second);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeVertexBufferPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a vertex layout failed to build";

		// Clip space, Y up, on an 8x8 target: x = px / 4 - 1, y = 1 - py / 4. Both triangles are cut
		// by the diagonal five pixels out from their corner, which passes half a pixel clear of every
		// pixel centre, so coverage is decided by the geometry and not by a rounding tie.
		// [0..2] is the red triangle in the top-left corner, [3..5] the green one in the bottom-right.
		const Vertex verts[6] = {
			{ -1.00f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{ -1.00f, -0.25f, 1.f, 0.f, 0.f, 1.f },
			{  0.25f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{  1.00f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  0.25f, 0.f, 1.f, 0.f, 1.f },
			{ -0.25f, -1.00f, 0.f, 1.f, 0.f, 1.f },
		};

		BufferDesc bufDesc;
		bufDesc.size = sizeof(verts);
		bufDesc.usage = BufferUsage::Vertex;
		bufDesc.cpuAccess = CpuAccess::Write;
		bufDesc.debugName = "rhi_test_vertex_offset_buffer";
		const auto vbo = rhi().createBuffer(bufDesc, verts);
		ASSERT_TRUE(vbo);

		const auto firstFb = makeTargetFramebuffer(rhi(), pass, first);
		const auto secondFb = makeTargetFramebuffer(rhi(), pass, second);
		ASSERT_TRUE(firstFb);
		ASSERT_TRUE(secondFb);

		drawThreeVerticesFrom(rhi(), pass, firstFb, pipeline, vbo, 0);
		drawThreeVerticesFrom(rhi(), pass, secondFb, pipeline, vbo, 3u * sizeof(Vertex));

		std::vector<uint8_t> firstPixels;
		std::vector<uint8_t> secondPixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(first, firstPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(second, secondPixels, w, h));
		ASSERT_FALSE(firstPixels.empty());
		ASSERT_FALSE(secondPixels.empty());

		const Rgba red{ 255, 0, 0, 255 };
		const Rgba green{ 0, 255, 0, 255 };
		const Rgba clear{ 0, 0, 0, 255 };

		// The offset-0 draw is the control: it has to be the first triangle alone, or the second
		// draw has nothing to differ from.
		EXPECT_EQ(pixelAt(firstPixels, 1, 1), red) << "offset 0 did not draw the first triangle";
		EXPECT_EQ(pixelAt(firstPixels, 6, 6), clear) << "offset 0 reached the second triangle's corner";

		// The whole point: three bytes-worth of vertices further in is a different triangle, in the
		// opposite corner and in the other colour. An ignored offset would repeat the picture above.
		EXPECT_EQ(pixelAt(secondPixels, 6, 6), green) << "a non-zero offset did not select the second triangle";
		EXPECT_EQ(pixelAt(secondPixels, 1, 1), clear) << "a non-zero offset still drew the first triangle";
	}

	// An indexed draw has three ways to pick its vertices and all three have to mean the same thing on
	// both backends: the index type the buffer is read with, the firstIndex the draw starts at, and the
	// vertexOffset added to every index it reads. The index buffer here is deliberately not the identity
	// permutation — index triple 0 names vertices 3..5 — so a backend that ignored the index buffer and
	// drew vertices 0..2 in order paints the wrong corner instead of passing by accident. OpenGL reaches
	// these through glDrawElementsInstanced(BaseVertex) with a byte offset it computes itself, Vulkan
	// hands them straight to vkCmdDrawIndexed, so the arithmetic is genuinely different on the two sides.
	TEST_P(RhiBackendTest, IndexedDrawTypeFirstIndexAndVertexOffsetSelectTheTriangle) {
		const auto uint16Target = makeTarget(rhi());
		const auto uint32Target = makeTarget(rhi());
		const auto firstIndexTarget = makeTarget(rhi());
		const auto vertexOffsetTarget = makeTarget(rhi());
		ASSERT_TRUE(uint16Target);
		ASSERT_TRUE(uint32Target);
		ASSERT_TRUE(firstIndexTarget);
		ASSERT_TRUE(vertexOffsetTarget);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeVertexBufferPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a vertex layout failed to build";

		// Clip space, Y up, on an 8x8 target: x = px / 4 - 1, y = 1 - py / 4. Three corner triangles,
		// each cut by the diagonal five pixels out from its corner so that no pixel centre sits on an
		// edge. [0..2] red top-left, [3..5] green bottom-right, [6..8] blue top-right; the three corners
		// share no pixel, so any pair of them is told apart by position as well as by colour.
		const Vertex verts[9] = {
			{ -1.00f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{ -1.00f, -0.25f, 1.f, 0.f, 0.f, 1.f },
			{  0.25f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{  1.00f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  0.25f, 0.f, 1.f, 0.f, 1.f },
			{ -0.25f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  1.00f, 0.f, 0.f, 1.f, 1.f },
			{  1.00f, -0.25f, 0.f, 0.f, 1.f, 1.f },
			{ -0.25f,  1.00f, 0.f, 0.f, 1.f, 1.f },
		};

		// Swapped on purpose: indices 0..2 name the green triangle and indices 3..5 the red one, so
		// "which triangle appeared" answers "were the indices read at all" and "did firstIndex move".
		const uint16_t indices16[6] = { 3, 4, 5, 0, 1, 2 };
		const uint32_t indices32[6] = { 3, 4, 5, 0, 1, 2 };

		BufferDesc vboDesc;
		vboDesc.size = sizeof(verts);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		vboDesc.debugName = "rhi_test_indexed_vertex_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts);
		ASSERT_TRUE(vbo);

		BufferDesc iboDesc;
		iboDesc.size = sizeof(indices16);
		iboDesc.usage = BufferUsage::Index;
		iboDesc.cpuAccess = CpuAccess::Write;
		iboDesc.debugName = "rhi_test_index_buffer_u16";
		const auto ibo16 = rhi().createBuffer(iboDesc, indices16);
		ASSERT_TRUE(ibo16);

		iboDesc.size = sizeof(indices32);
		iboDesc.debugName = "rhi_test_index_buffer_u32";
		const auto ibo32 = rhi().createBuffer(iboDesc, indices32);
		ASSERT_TRUE(ibo32);

		const auto uint16Fb = makeTargetFramebuffer(rhi(), pass, uint16Target);
		const auto uint32Fb = makeTargetFramebuffer(rhi(), pass, uint32Target);
		const auto firstIndexFb = makeTargetFramebuffer(rhi(), pass, firstIndexTarget);
		const auto vertexOffsetFb = makeTargetFramebuffer(rhi(), pass, vertexOffsetTarget);
		ASSERT_TRUE(uint16Fb);
		ASSERT_TRUE(uint32Fb);
		ASSERT_TRUE(firstIndexFb);
		ASSERT_TRUE(vertexOffsetFb);

		// The same three indices, once as 16-bit and once as 32-bit: same geometry, so same picture.
		drawIndexedTriangle(rhi(), pass, uint16Fb, pipeline, vbo, ibo16, IndexType::Uint16, 0, 0);
		drawIndexedTriangle(rhi(), pass, uint32Fb, pipeline, vbo, ibo32, IndexType::Uint32, 0, 0);
		// firstIndex 3 starts at the second index triple, which names the red triangle.
		drawIndexedTriangle(rhi(), pass, firstIndexFb, pipeline, vbo, ibo16, IndexType::Uint16, 3, 0);
		// vertexOffset 3 leaves the indices alone and shifts what they resolve to: 3,4,5 becomes 6,7,8,
		// the blue triangle. Distinct from the firstIndex draw above, so the two cannot be confused.
		drawIndexedTriangle(rhi(), pass, vertexOffsetFb, pipeline, vbo, ibo16, IndexType::Uint16, 0, 3);

		std::vector<uint8_t> uint16Pixels;
		std::vector<uint8_t> uint32Pixels;
		std::vector<uint8_t> firstIndexPixels;
		std::vector<uint8_t> vertexOffsetPixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(uint16Target, uint16Pixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(uint32Target, uint32Pixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(firstIndexTarget, firstIndexPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(vertexOffsetTarget, vertexOffsetPixels, w, h));
		ASSERT_FALSE(uint16Pixels.empty());
		ASSERT_FALSE(uint32Pixels.empty());
		ASSERT_FALSE(firstIndexPixels.empty());
		ASSERT_FALSE(vertexOffsetPixels.empty());

		const Rgba red{ 255, 0, 0, 255 };
		const Rgba green{ 0, 255, 0, 255 };
		const Rgba blue{ 0, 0, 255, 255 };
		const Rgba clear{ 0, 0, 0, 255 };

		// Uint16 is the control for everything below. Green in the bottom-right corner and nothing in
		// the top-left is the only outcome that says the indices were read: an index buffer skipped in
		// favour of vertices 0..2 in order would paint the top-left corner red instead.
		EXPECT_EQ(pixelAt(uint16Pixels, 6, 6), green) << "the Uint16 indices did not select the second triangle";
		EXPECT_EQ(pixelAt(uint16Pixels, 1, 1), clear) << "the Uint16 draw painted the triangle the indices do not name";

		// The same numbers widened to 32 bits describe the same triangle. A backend that read a 32-bit
		// buffer as 16-bit would see 3, 0, 4, 0, 5, 0 and paint somewhere else entirely.
		EXPECT_EQ(pixelAt(uint32Pixels, 6, 6), green) << "Uint32 indices did not resolve to the same triangle as Uint16";
		EXPECT_EQ(pixelAt(uint32Pixels, 1, 1), clear) << "the Uint32 draw painted the triangle the indices do not name";

		// firstIndex is counted in indices, not bytes, and is added on top of the bind offset: three in
		// means the second triple. An ignored firstIndex repeats the green picture above.
		EXPECT_EQ(pixelAt(firstIndexPixels, 1, 1), red) << "firstIndex did not skip to the second index triple";
		EXPECT_EQ(pixelAt(firstIndexPixels, 6, 6), clear) << "firstIndex was ignored: still the first triple's triangle";

		// vertexOffset is added to each index after it is fetched, so the same triple lands on a third
		// triangle. Ignored, this draw would be the control's green bottom-right corner; applied to the
		// index buffer position instead of the vertex it would be the red top-left one.
		EXPECT_EQ(pixelAt(vertexOffsetPixels, 6, 1), blue) << "vertexOffset did not shift which vertices the indices resolve to";
		EXPECT_EQ(pixelAt(vertexOffsetPixels, 6, 6), clear) << "vertexOffset was ignored: still the unshifted triangle";
		EXPECT_EQ(pixelAt(vertexOffsetPixels, 1, 1), clear) << "vertexOffset behaved like firstIndex";
	}

	// Instancing is three separate claims and the picture has to answer all three. A binding declared
	// PerInstance advances once per instance, not once per vertex, so one copy of the triangle keeps
	// one offset and one colour; instanceCount decides how many copies exist; firstInstance decides
	// which record the first copy reads. The four records sit two pixels apart and carry four
	// different colours, so every wrong answer lands on a different pixel or a different colour
	// rather than on something that could pass by luck. The backends reach this by different routes:
	// OpenGL sets a divisor on the pipeline's VAO and calls glDrawArraysInstanced, Vulkan puts the
	// rate in the pipeline's vertex input state and hands both numbers to vkCmdDraw.
	TEST_P(RhiBackendTest, InstancedDrawAdvancesPerInstanceBindingAndHonoursInstanceCountAndFirstInstance) {
		const auto countTarget = makeTarget(rhi());
		const auto firstTarget = makeTarget(rhi());
		ASSERT_TRUE(countTarget);
		ASSERT_TRUE(firstTarget);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeInstancedPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a per-instance binding failed to build";

		// Clip space, Y up, on an 8x8 target: x = px / 4 - 1, y = 1 - py / 4. A right triangle in the
		// top-left corner, two pixels wide and four tall, whose hypotenuse runs from pixel (0, 4) to
		// (2, 0): it covers pixel centres (0, 0) and (1, 0) and stops well short of column 2, so an
		// instance never bleeds into the next instance's slot. White, so the pixel reports the
		// per-instance colour unchanged while binding 0 still has to be read for the shape to exist.
		const Vertex verts[3] = {
			{ -1.0f,  1.0f, 1.f, 1.f, 1.f, 1.f },
			{ -1.0f,  0.0f, 1.f, 1.f, 1.f, 1.f },
			{ -0.5f,  1.0f, 1.f, 1.f, 1.f, 1.f },
		};

		// Four slots, each 0.5 in clip space, which is two pixels. Record i lands on pixels (2i, 0)
		// and (2i + 1, 0) and nowhere else, so "which record was read" is answered by position as
		// well as by colour. Record 3 exists only to be left out by instanceCount.
		const Instance instances[4] = {
			{ 0.0f, 0.f, 1.f, 0.f, 0.f, 1.f },
			{ 0.5f, 0.f, 0.f, 1.f, 0.f, 1.f },
			{ 1.0f, 0.f, 0.f, 0.f, 1.f, 1.f },
			{ 1.5f, 0.f, 1.f, 1.f, 0.f, 1.f },
		};

		BufferDesc vboDesc;
		vboDesc.size = sizeof(verts);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		vboDesc.debugName = "rhi_test_instanced_vertex_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts);
		ASSERT_TRUE(vbo);

		BufferDesc instDesc;
		instDesc.size = sizeof(instances);
		instDesc.usage = BufferUsage::Vertex;
		instDesc.cpuAccess = CpuAccess::Write;
		instDesc.debugName = "rhi_test_instance_buffer";
		const auto instanceBuffer = rhi().createBuffer(instDesc, instances);
		ASSERT_TRUE(instanceBuffer);

		const auto countFb = makeTargetFramebuffer(rhi(), pass, countTarget);
		const auto firstFb = makeTargetFramebuffer(rhi(), pass, firstTarget);
		ASSERT_TRUE(countFb);
		ASSERT_TRUE(firstFb);

		// Three instances from record 0: slots 0, 1 and 2 painted, slot 3 left alone.
		drawInstances(rhi(), pass, countFb, pipeline, vbo, instanceBuffer, 3, 0);
		// Two instances from record 1: the same buffer, shifted by one record, so slot 0 goes dark
		// and slot 3 stays dark.
		drawInstances(rhi(), pass, firstFb, pipeline, vbo, instanceBuffer, 2, 1);

		std::vector<uint8_t> countPixels;
		std::vector<uint8_t> firstPixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(countTarget, countPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(firstTarget, firstPixels, w, h));
		ASSERT_FALSE(countPixels.empty());
		ASSERT_FALSE(firstPixels.empty());

		const Rgba red{ 255, 0, 0, 255 };
		const Rgba green{ 0, 255, 0, 255 };
		const Rgba blue{ 0, 0, 255, 255 };
		const Rgba clear{ 0, 0, 0, 255 };

		// Three instances, three records, three slots. A binding read at the per-vertex rate would
		// hand the first instance's three vertices the offsets of records 0, 1 and 2 and stretch the
		// triangle into a shape that covers none of these pixels, so the first line alone rules the
		// divisor out; the colours then say each slot read the record that belongs to it.
		EXPECT_EQ(pixelAt(countPixels, 0, 0), red) << "the first instance did not read the first record";
		EXPECT_EQ(pixelAt(countPixels, 2, 0), green) << "the second instance did not read the second record";
		EXPECT_EQ(pixelAt(countPixels, 4, 0), blue) << "the third instance did not read the third record";
		// The other pixel of the first instance's own triangle: one instance is one whole triangle,
		// all of it in that instance's colour, not a per-vertex smear across records.
		EXPECT_EQ(pixelAt(countPixels, 1, 0), red) << "one instance did not draw one whole triangle in one record's colour";
		// Record 3 is in the buffer and instanceCount stops short of it. Painted here means the count
		// was ignored and the whole buffer was drawn.
		EXPECT_EQ(pixelAt(countPixels, 6, 0), clear) << "instanceCount did not limit how many instances were drawn";

		// firstInstance moves the window over the same buffer: two instances starting at record 1 are
		// the green and blue slots, and the red one is gone. Ignored, this is the picture above.
		EXPECT_EQ(pixelAt(firstPixels, 2, 0), green) << "firstInstance did not start at the second record";
		EXPECT_EQ(pixelAt(firstPixels, 4, 0), blue) << "the second instance did not read the record after firstInstance";
		EXPECT_EQ(pixelAt(firstPixels, 0, 0), clear) << "firstInstance was ignored: the skipped record still drew";
		EXPECT_EQ(pixelAt(firstPixels, 6, 0), clear) << "firstInstance shifted the count as well as the start";
	}

	// An indirect draw takes its arguments from a buffer, and offset says which record in that buffer
	// it starts at — in bytes, like every other buffer offset in the interface, not in records. Four
	// records here name four different corner triangles, so a draw that read the wrong record paints a
	// different corner in a different colour instead of something that could pass by luck: an ignored
	// offset falls back to record 0, an offset counted in records instead of bytes runs off the end,
	// and a record read with the wrong field order draws no triangle at all. drawCount rides along
	// because it costs one more target: two records from the offset, and the record beyond them has to
	// stay unpainted. The two backends genuinely take different routes — OpenGL reinterprets the offset
	// as a pointer into GL_DRAW_INDIRECT_BUFFER and switches to glMultiDrawArraysIndirect once
	// drawCount exceeds one, Vulkan hands offset, drawCount and stride straight to vkCmdDrawIndirect.
	TEST_P(RhiBackendTest, IndirectDrawReadsTheRecordAtTheGivenOffsetAndDrawsDrawCountOfThem) {
		const auto firstTarget = makeTarget(rhi());
		const auto offsetTarget = makeTarget(rhi());
		const auto multiTarget = makeTarget(rhi());
		ASSERT_TRUE(firstTarget);
		ASSERT_TRUE(offsetTarget);
		ASSERT_TRUE(multiTarget);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeVertexBufferPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a vertex layout failed to build";

		// Clip space, Y up, on an 8x8 target: x = px / 4 - 1, y = 1 - py / 4. One triangle per corner,
		// each cut by the diagonal five pixels out from its corner so that no pixel centre sits on an
		// edge. [0..2] red top-left, [3..5] green bottom-right, [6..8] blue top-right, [9..11] yellow
		// bottom-left; no two of them share a pixel, so every record is told apart by position as well
		// as by colour.
		const Vertex verts[12] = {
			{ -1.00f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{ -1.00f, -0.25f, 1.f, 0.f, 0.f, 1.f },
			{  0.25f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{  1.00f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  0.25f, 0.f, 1.f, 0.f, 1.f },
			{ -0.25f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  1.00f, 0.f, 0.f, 1.f, 1.f },
			{  1.00f, -0.25f, 0.f, 0.f, 1.f, 1.f },
			{ -0.25f,  1.00f, 0.f, 0.f, 1.f, 1.f },
			{ -1.00f, -1.00f, 1.f, 1.f, 0.f, 1.f },
			{ -1.00f,  0.25f, 1.f, 1.f, 0.f, 1.f },
			{  0.25f, -1.00f, 1.f, 1.f, 0.f, 1.f },
		};

		// One record per triangle, differing only in firstVertex, so which corner appears answers which
		// record was read. Record 3 exists to be left out by drawCount.
		const DrawArgs args[4] = {
			{ 3, 1, 0, 0 },
			{ 3, 1, 3, 0 },
			{ 3, 1, 6, 0 },
			{ 3, 1, 9, 0 },
		};

		BufferDesc vboDesc;
		vboDesc.size = sizeof(verts);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		vboDesc.debugName = "rhi_test_indirect_vertex_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts);
		ASSERT_TRUE(vbo);

		BufferDesc argsDesc;
		argsDesc.size = sizeof(args);
		// Indirect, not Vertex: the backends put the buffer somewhere else for it (Vulkan asks for
		// VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, OpenGL binds it to GL_DRAW_INDIRECT_BUFFER), so a usage
		// that failed to reach the allocation would show up here rather than in the engine.
		argsDesc.usage = BufferUsage::Indirect;
		argsDesc.cpuAccess = CpuAccess::Write;
		argsDesc.debugName = "rhi_test_indirect_args_buffer";
		const auto argsBuffer = rhi().createBuffer(argsDesc, args);
		ASSERT_TRUE(argsBuffer) << "a buffer declared BufferUsage::Indirect failed to allocate";

		const auto firstFb = makeTargetFramebuffer(rhi(), pass, firstTarget);
		const auto offsetFb = makeTargetFramebuffer(rhi(), pass, offsetTarget);
		const auto multiFb = makeTargetFramebuffer(rhi(), pass, multiTarget);
		ASSERT_TRUE(firstFb);
		ASSERT_TRUE(offsetFb);
		ASSERT_TRUE(multiFb);

		// Offset 0 is the control: record 0 alone, or the offset draw has nothing to differ from.
		drawIndirectFrom(rhi(), pass, firstFb, pipeline, vbo, argsBuffer, 0, 1);
		// One record in, which is sizeof(DrawArgs) bytes in: the green triangle.
		drawIndirectFrom(rhi(), pass, offsetFb, pipeline, vbo, argsBuffer, sizeof(DrawArgs), 1);
		// Two records from that same place: green and blue, with red skipped and yellow out of range.
		drawIndirectFrom(rhi(), pass, multiFb, pipeline, vbo, argsBuffer, sizeof(DrawArgs), 2);

		std::vector<uint8_t> firstPixels;
		std::vector<uint8_t> offsetPixels;
		std::vector<uint8_t> multiPixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(firstTarget, firstPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(offsetTarget, offsetPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(multiTarget, multiPixels, w, h));
		ASSERT_FALSE(firstPixels.empty());
		ASSERT_FALSE(offsetPixels.empty());
		ASSERT_FALSE(multiPixels.empty());

		const Rgba red{ 255, 0, 0, 255 };
		const Rgba green{ 0, 255, 0, 255 };
		const Rgba blue{ 0, 0, 255, 255 };
		const Rgba clear{ 0, 0, 0, 255 };

		// The arguments were read at all, and read as this struct: a record whose fields landed in any
		// other order would ask for a vertexCount or an instanceCount of 0 or 3-going-on-nonsense and
		// paint nothing here.
		EXPECT_EQ(pixelAt(firstPixels, 1, 1), red) << "the record at offset 0 did not draw its triangle";
		EXPECT_EQ(pixelAt(firstPixels, 6, 6), clear) << "offset 0 reached a record other than the first";

		// The whole point: one record further in is a different triangle, in a different corner and a
		// different colour. An ignored offset repeats the control's red corner.
		EXPECT_EQ(pixelAt(offsetPixels, 6, 6), green) << "a non-zero offset did not select the second record";
		EXPECT_EQ(pixelAt(offsetPixels, 1, 1), clear) << "a non-zero offset still drew the first record";
		// drawCount is 1 here, so only the record at the offset may appear.
		EXPECT_EQ(pixelAt(offsetPixels, 6, 1), clear) << "drawCount 1 drew more than the record at the offset";

		// drawCount 2 walks forward from the offset one stride at a time. Both records have to arrive:
		// a multi-draw that read the same record twice, or walked with the wrong stride, misses one.
		EXPECT_EQ(pixelAt(multiPixels, 6, 6), green) << "the first of the two records did not draw";
		EXPECT_EQ(pixelAt(multiPixels, 6, 1), blue) << "the second record of a drawCount 2 draw did not draw";
		// Record 0 sits before the offset and record 3 after the count. Either corner painted means the
		// multi-draw path ignored the offset it was given or ran past the records it was asked for.
		EXPECT_EQ(pixelAt(multiPixels, 1, 1), clear) << "the multi-draw ignored the offset and started at record 0";
		EXPECT_EQ(pixelAt(multiPixels, 1, 6), clear) << "the multi-draw ran past drawCount into the fourth record";
	}

	// The indexed indirect record is a different shape from the non-indexed one — five fields instead
	// of four, hence a default stride of 20 instead of 16 — and two of them, firstIndex and
	// vertexOffset, have no counterpart there at all. The direct-draw equivalents are already pinned by
	// IndexedDrawTypeFirstIndexAndVertexOffsetSelectTheTriangle; the question here is whether the same
	// two numbers, fetched out of a buffer by the GPU rather than passed down as arguments, still mean
	// the same thing. The index buffer is deliberately not the identity permutation — index triple 0
	// names vertices 3..5 — so a draw that ignored it and took vertices in order paints the wrong corner
	// instead of passing by accident. offset picks the record, four records name four different corners,
	// and drawCount 2 rides along because it costs one more target and is the only branch of OpenGL's
	// execDrawIndexedIndirect that reads stride: glDrawElementsIndirect ignores stride entirely, only
	// glMultiDrawElementsIndirect is handed it. Vulkan takes offset, drawCount and stride straight to
	// vkCmdDrawIndexedIndirect either way.
	TEST_P(RhiBackendTest, IndexedIndirectDrawReadsFirstIndexAndVertexOffsetFromTheRecordAtTheGivenOffset) {
		const auto firstTarget = makeTarget(rhi());
		const auto firstIndexTarget = makeTarget(rhi());
		const auto vertexOffsetTarget = makeTarget(rhi());
		const auto multiTarget = makeTarget(rhi());
		const auto negativeTarget = makeTarget(rhi());
		ASSERT_TRUE(firstTarget);
		ASSERT_TRUE(firstIndexTarget);
		ASSERT_TRUE(vertexOffsetTarget);
		ASSERT_TRUE(multiTarget);
		ASSERT_TRUE(negativeTarget);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeVertexBufferPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a vertex layout failed to build";

		// Clip space, Y up, on an 8x8 target: x = px / 4 - 1, y = 1 - py / 4. One triangle per corner,
		// each cut by the diagonal five pixels out from its corner so that no pixel centre sits on an
		// edge. [0..2] red top-left, [3..5] green bottom-right, [6..8] blue top-right, [9..11] yellow
		// bottom-left; no two of them share a pixel, so every record is told apart by position as well
		// as by colour. Same geometry as the direct indexed test, so the two can be compared by eye.
		const Vertex verts[12] = {
			{ -1.00f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{ -1.00f, -0.25f, 1.f, 0.f, 0.f, 1.f },
			{  0.25f,  1.00f, 1.f, 0.f, 0.f, 1.f },
			{  1.00f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  0.25f, 0.f, 1.f, 0.f, 1.f },
			{ -0.25f, -1.00f, 0.f, 1.f, 0.f, 1.f },
			{  1.00f,  1.00f, 0.f, 0.f, 1.f, 1.f },
			{  1.00f, -0.25f, 0.f, 0.f, 1.f, 1.f },
			{ -0.25f,  1.00f, 0.f, 0.f, 1.f, 1.f },
			{ -1.00f, -1.00f, 1.f, 1.f, 0.f, 1.f },
			{ -1.00f,  0.25f, 1.f, 1.f, 0.f, 1.f },
			{  0.25f, -1.00f, 1.f, 1.f, 0.f, 1.f },
		};

		// Triple 0 names the green triangle, triple 1 the red one and triple 2 the yellow one — none of
		// them in the order the vertices are stored, so "which corner appeared" answers "were the
		// indices read at all" before it answers anything about the record's fields.
		const uint16_t indices[9] = { 3, 4, 5, 0, 1, 2, 9, 10, 11 };

		// Five records, each naming a different corner, so which corner appears says which record was
		// read and how its fields were applied:
		//   0  triple 0 straight through                       -> green  bottom-right
		//   1  firstIndex 3, so triple 1                       -> red    top-left
		//   2  vertexOffset 3, so triple 0's 3,4,5 become 6,7,8 -> blue   top-right
		//   3  firstIndex 6, so triple 2                       -> yellow bottom-left
		//   4  triple 2 with vertexOffset -6: 9,10,11 -> 3,4,5 -> green  bottom-right
		// Record 2 is why vertexOffset 3 rather than 6: a vertexOffset mistakenly applied as a
		// firstIndex would give triple 1 and paint red, which is a different corner from blue. At 6 the
		// two mistakes would land on the same yellow corner and the assertion would not bite.
		const DrawIndexedArgs args[5] = {
			{ 3, 1, 0,  0, 0 },
			{ 3, 1, 3,  0, 0 },
			{ 3, 1, 0,  3, 0 },
			{ 3, 1, 6,  0, 0 },
			{ 3, 1, 6, -6, 0 },
		};

		BufferDesc vboDesc;
		vboDesc.size = sizeof(verts);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		vboDesc.debugName = "rhi_test_indexed_indirect_vertex_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts);
		ASSERT_TRUE(vbo);

		BufferDesc iboDesc;
		iboDesc.size = sizeof(indices);
		iboDesc.usage = BufferUsage::Index;
		iboDesc.cpuAccess = CpuAccess::Write;
		iboDesc.debugName = "rhi_test_indexed_indirect_index_buffer";
		const auto ibo = rhi().createBuffer(iboDesc, indices);
		ASSERT_TRUE(ibo);

		BufferDesc argsDesc;
		argsDesc.size = sizeof(args);
		// Indirect, not Index: the backends put the buffer somewhere else for it (Vulkan asks for
		// VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, OpenGL binds it to GL_DRAW_INDIRECT_BUFFER), and the
		// OpenGL backend asserts on the usage of the buffer it is handed as an index buffer, so the two
		// roles cannot be served by one allocation.
		argsDesc.usage = BufferUsage::Indirect;
		argsDesc.cpuAccess = CpuAccess::Write;
		argsDesc.debugName = "rhi_test_indexed_indirect_args_buffer";
		const auto argsBuffer = rhi().createBuffer(argsDesc, args);
		ASSERT_TRUE(argsBuffer) << "a buffer declared BufferUsage::Indirect failed to allocate";

		const auto firstFb = makeTargetFramebuffer(rhi(), pass, firstTarget);
		const auto firstIndexFb = makeTargetFramebuffer(rhi(), pass, firstIndexTarget);
		const auto vertexOffsetFb = makeTargetFramebuffer(rhi(), pass, vertexOffsetTarget);
		const auto multiFb = makeTargetFramebuffer(rhi(), pass, multiTarget);
		const auto negativeFb = makeTargetFramebuffer(rhi(), pass, negativeTarget);
		ASSERT_TRUE(firstFb);
		ASSERT_TRUE(firstIndexFb);
		ASSERT_TRUE(vertexOffsetFb);
		ASSERT_TRUE(multiFb);
		ASSERT_TRUE(negativeFb);

		// Offset 0 is the control: record 0 alone, or the offset draws have nothing to differ from.
		drawIndexedIndirectFrom(rhi(), pass, firstFb, pipeline, vbo, ibo, argsBuffer, 0, 1);
		// One record in, which is sizeof(DrawIndexedArgs) bytes in, not one: the firstIndex record.
		drawIndexedIndirectFrom(rhi(), pass, firstIndexFb, pipeline, vbo, ibo, argsBuffer, sizeof(DrawIndexedArgs), 1);
		// Two records in: the vertexOffset record.
		drawIndexedIndirectFrom(rhi(), pass, vertexOffsetFb, pipeline, vbo, ibo, argsBuffer, 2 * sizeof(DrawIndexedArgs), 1);
		// Two records from one record in: the firstIndex and vertexOffset records, with record 0 before
		// the offset and record 3 past the count.
		drawIndexedIndirectFrom(rhi(), pass, multiFb, pipeline, vbo, ibo, argsBuffer, sizeof(DrawIndexedArgs), 2);
		// Four records in: the one whose vertexOffset is negative.
		drawIndexedIndirectFrom(rhi(), pass, negativeFb, pipeline, vbo, ibo, argsBuffer, 4 * sizeof(DrawIndexedArgs), 1);

		std::vector<uint8_t> firstPixels;
		std::vector<uint8_t> firstIndexPixels;
		std::vector<uint8_t> vertexOffsetPixels;
		std::vector<uint8_t> multiPixels;
		std::vector<uint8_t> negativePixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(firstTarget, firstPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(firstIndexTarget, firstIndexPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(vertexOffsetTarget, vertexOffsetPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(multiTarget, multiPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(negativeTarget, negativePixels, w, h));
		ASSERT_FALSE(firstPixels.empty());
		ASSERT_FALSE(firstIndexPixels.empty());
		ASSERT_FALSE(vertexOffsetPixels.empty());
		ASSERT_FALSE(multiPixels.empty());
		ASSERT_FALSE(negativePixels.empty());

		const Rgba red{ 255, 0, 0, 255 };
		const Rgba green{ 0, 255, 0, 255 };
		const Rgba blue{ 0, 0, 255, 255 };
		const Rgba yellow{ 255, 255, 0, 255 };
		const Rgba clear{ 0, 0, 0, 255 };

		// The record was read at all, read as five fields in this order, and the index buffer went with
		// it: a record whose fields landed in any other order asks for an indexCount or an instanceCount
		// of 0 and paints nothing, and a draw that skipped the indices takes vertices 0..2 and paints the
		// red corner rather than the green one.
		EXPECT_EQ(pixelAt(firstPixels, 6, 6), green) << "the record at offset 0 did not draw the triangle its indices name";
		EXPECT_EQ(pixelAt(firstPixels, 1, 1), clear) << "the indices were skipped and vertices 0..2 drawn in order";

		// offset is in bytes and the record it lands on decides the picture. An ignored offset repeats
		// the control's green corner; an offset counted in records lands inside record 0 and reads five
		// words of nonsense straddling the record boundary.
		EXPECT_EQ(pixelAt(firstIndexPixels, 1, 1), red) << "the record's firstIndex did not skip to the second index triple";
		EXPECT_EQ(pixelAt(firstIndexPixels, 6, 6), clear) << "a non-zero offset still drew the first record";

		// vertexOffset is added to each index after it is fetched, exactly as in the direct draw, so the
		// same triple lands on a third triangle. Ignored, this is the control's green corner; applied to
		// the index buffer position instead of to the vertex it names, it is the red one.
		EXPECT_EQ(pixelAt(vertexOffsetPixels, 6, 1), blue) << "the record's vertexOffset did not shift which vertices the indices resolve to";
		EXPECT_EQ(pixelAt(vertexOffsetPixels, 6, 6), clear) << "the record's vertexOffset was ignored";
		EXPECT_EQ(pixelAt(vertexOffsetPixels, 1, 1), clear) << "the record's vertexOffset behaved like a firstIndex";

		// drawCount 2 walks forward from the offset one stride at a time, which on OpenGL is a different
		// entry point from the single-record case and the only one that is told the stride at all. Both
		// records have to arrive: a multi-draw that read the same record twice, or walked with the
		// 16-byte stride of the non-indexed record, misses one and lands mid-record on the other.
		EXPECT_EQ(pixelAt(multiPixels, 1, 1), red) << "the first of the two records did not draw";
		EXPECT_EQ(pixelAt(multiPixels, 6, 1), blue) << "the second record of a drawCount 2 draw did not draw";
		// Record 0 sits before the offset and record 3 after the count. Either corner painted means the
		// multi-draw path ignored the offset it was given or ran past the records it was asked for.
		EXPECT_EQ(pixelAt(multiPixels, 6, 6), clear) << "the multi-draw ignored the offset and started at record 0";
		EXPECT_EQ(pixelAt(multiPixels, 1, 6), clear) << "the multi-draw ran past drawCount into the fourth record";

		// vertexOffset is signed in both specs and the record's fourth word is written here as a two's
		// complement -6, so a triple naming vertices 9..11 has to resolve to 3..5. Nothing in the RHI
		// reads this field — it is the GPU that adds it — so this pins the one place the declared
		// signedness could still show: a backend or driver that clamped a negative offset to zero would
		// leave the yellow corner its indices name untouched instead.
		EXPECT_EQ(pixelAt(negativePixels, 6, 6), green) << "a negative vertexOffset did not shift the indices down";
		EXPECT_EQ(pixelAt(negativePixels, 1, 6), clear) << "a negative vertexOffset was dropped: the unshifted triangle still drew";
	}

	// Two of a DrawArgs record's four words have never been under test. Every indirect draw so far ran
	// against a pipeline with no per-instance binding, where instanceCount only decides how many times
	// the same pixels are painted the same colour and firstInstance decides nothing at all, so a
	// backend could drop either word and the pictures would not change. Binding the instanced pipeline
	// gives them somewhere to show: four records two pixels apart in four colours, so the count is read
	// off how many slots were painted and the start off which ones. The direct-draw equivalents are
	// already pinned by InstancedDrawAdvancesPerInstanceBindingAndHonoursInstanceCountAndFirstInstance
	// and the answer there does not carry over — that path passes the two numbers down as arguments,
	// and OpenGL was caught dropping firstInstance on exactly that route by calling the non-base-instance
	// entry point. Here nothing in the RHI reads either word: the GPU fetches them out of the buffer,
	// through glDrawArraysIndirect / glMultiDrawArraysIndirect on one side and vkCmdDrawIndirect on the
	// other, so whether the two agree is a separate question from whether the direct paths do.
	TEST_P(RhiBackendTest, IndirectRecordsHonourInstanceCountAndFirstInstanceAgainstAnInstancedPipeline) {
		const auto countTarget = makeTarget(rhi());
		const auto firstInstanceTarget = makeTarget(rhi());
		const auto multiTarget = makeTarget(rhi());
		ASSERT_TRUE(countTarget);
		ASSERT_TRUE(firstInstanceTarget);
		ASSERT_TRUE(multiTarget);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 1.f });
		ASSERT_TRUE(pass);
		const auto pipeline = makeInstancedPipeline(rhi(), pass);
		ASSERT_TRUE(pipeline) << "pipeline with a per-instance binding failed to build";

		// Same triangle as the direct instanced test, for the same reasons: a right corner two pixels
		// wide and four tall that covers pixel centres (0, 0) and (1, 0) and stops well short of column
		// 2, so one instance never bleeds into the next instance's slot. White, so the pixel reports the
		// per-instance colour unchanged while binding 0 still has to be read for the shape to exist.
		const Vertex verts[3] = {
			{ -1.0f,  1.0f, 1.f, 1.f, 1.f, 1.f },
			{ -1.0f,  0.0f, 1.f, 1.f, 1.f, 1.f },
			{ -0.5f,  1.0f, 1.f, 1.f, 1.f, 1.f },
		};

		// Four slots, each 0.5 in clip space, which is two pixels. Record i lands on pixels (2i, 0) and
		// (2i + 1, 0) and nowhere else, so "which instances ran" is answered by position as well as by
		// colour.
		const Instance instances[4] = {
			{ 0.0f, 0.f, 1.f, 0.f, 0.f, 1.f },
			{ 0.5f, 0.f, 0.f, 1.f, 0.f, 1.f },
			{ 1.0f, 0.f, 0.f, 0.f, 1.f, 1.f },
			{ 1.5f, 0.f, 1.f, 1.f, 0.f, 1.f },
		};

		// Every record draws the same three vertices from the same place, so firstVertex explains
		// nothing here and the two fields under test are the only thing telling the pictures apart:
		//   0  two instances from 0 -> red and green, blue and yellow left dark
		//   1  one instance from 2  -> blue alone, both neighbours dark
		//   2  one instance from 3  -> yellow, and exists only to be left out by drawCount
		const DrawArgs args[3] = {
			{ 3, 2, 0, 0 },
			{ 3, 1, 0, 2 },
			{ 3, 1, 0, 3 },
		};

		BufferDesc vboDesc;
		vboDesc.size = sizeof(verts);
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::Write;
		vboDesc.debugName = "rhi_test_instanced_indirect_vertex_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts);
		ASSERT_TRUE(vbo);

		BufferDesc instDesc;
		instDesc.size = sizeof(instances);
		instDesc.usage = BufferUsage::Vertex;
		instDesc.cpuAccess = CpuAccess::Write;
		instDesc.debugName = "rhi_test_instanced_indirect_instance_buffer";
		const auto instanceBuffer = rhi().createBuffer(instDesc, instances);
		ASSERT_TRUE(instanceBuffer);

		BufferDesc argsDesc;
		argsDesc.size = sizeof(args);
		// Indirect, not Vertex: the backends put the buffer somewhere else for it, and the instance
		// buffer above is a Vertex allocation, so the two roles cannot be served by one buffer.
		argsDesc.usage = BufferUsage::Indirect;
		argsDesc.cpuAccess = CpuAccess::Write;
		argsDesc.debugName = "rhi_test_instanced_indirect_args_buffer";
		const auto argsBuffer = rhi().createBuffer(argsDesc, args);
		ASSERT_TRUE(argsBuffer) << "a buffer declared BufferUsage::Indirect failed to allocate";

		const auto countFb = makeTargetFramebuffer(rhi(), pass, countTarget);
		const auto firstInstanceFb = makeTargetFramebuffer(rhi(), pass, firstInstanceTarget);
		const auto multiFb = makeTargetFramebuffer(rhi(), pass, multiTarget);
		ASSERT_TRUE(countFb);
		ASSERT_TRUE(firstInstanceFb);
		ASSERT_TRUE(multiFb);

		// Record 0 alone: the instanceCount case, and the control the other two differ from.
		drawInstancedIndirectFrom(rhi(), pass, countFb, pipeline, vbo, instanceBuffer, argsBuffer, 0, 1);
		// Record 1 alone: the firstInstance case, one record in and so sizeof(DrawArgs) bytes in.
		drawInstancedIndirectFrom(rhi(), pass, firstInstanceFb, pipeline, vbo, instanceBuffer, argsBuffer, sizeof(DrawArgs), 1);
		// Records 0 and 1 together, which on OpenGL is the other entry point entirely: two records whose
		// counts and starts disagree, with record 2 past the count.
		drawInstancedIndirectFrom(rhi(), pass, multiFb, pipeline, vbo, instanceBuffer, argsBuffer, 0, 2);

		std::vector<uint8_t> countPixels;
		std::vector<uint8_t> firstInstancePixels;
		std::vector<uint8_t> multiPixels;
		int w = 0;
		int h = 0;
		ASSERT_TRUE(rhi().readbackTexture(countTarget, countPixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(firstInstanceTarget, firstInstancePixels, w, h));
		ASSERT_TRUE(rhi().readbackTexture(multiTarget, multiPixels, w, h));
		ASSERT_FALSE(countPixels.empty());
		ASSERT_FALSE(firstInstancePixels.empty());
		ASSERT_FALSE(multiPixels.empty());

		const Rgba red{ 255, 0, 0, 255 };
		const Rgba green{ 0, 255, 0, 255 };
		const Rgba blue{ 0, 0, 255, 255 };
		const Rgba clear{ 0, 0, 0, 255 };

		// A record asking for two instances has to run twice, off two consecutive per-instance records.
		// One slot painted means the count word was read as 1 or dropped; a per-vertex divisor would
		// stretch the triangle across three records and cover neither pixel named here.
		EXPECT_EQ(pixelAt(countPixels, 0, 0), red) << "the record's first instance did not read the first per-instance record";
		EXPECT_EQ(pixelAt(countPixels, 2, 0), green) << "the record's instanceCount of 2 did not run a second instance";
		// The count stops at two and there are four records in the buffer. Painted here means the count
		// word was ignored and the instance buffer drained instead.
		EXPECT_EQ(pixelAt(countPixels, 4, 0), clear) << "the record's instanceCount did not limit how many instances ran";
		EXPECT_EQ(pixelAt(countPixels, 6, 0), clear) << "the record's instanceCount did not limit how many instances ran";

		// firstInstance inside the record moves the window over the instance buffer without changing
		// how many instances run. Dropped, this is slot 0 in red — which is what OpenGL's direct draw
		// used to do before execDraw was moved onto the base-instance entry point.
		EXPECT_EQ(pixelAt(firstInstancePixels, 4, 0), blue) << "the record's firstInstance did not start at the third per-instance record";
		EXPECT_EQ(pixelAt(firstInstancePixels, 0, 0), clear) << "the record's firstInstance was dropped: the draw started at instance 0";
		EXPECT_EQ(pixelAt(firstInstancePixels, 2, 0), clear) << "the record's firstInstance landed on the wrong per-instance record";
		EXPECT_EQ(pixelAt(firstInstancePixels, 6, 0), clear) << "the record's firstInstance shifted the count as well as the start";

		// Two records in one draw, each with its own count and its own start, so a multi-draw that read
		// one record's two words and reused them for both misses a slot: record 0's fields applied twice
		// leave slot 2 dark, record 1's applied twice leave slots 0 and 1 dark.
		EXPECT_EQ(pixelAt(multiPixels, 0, 0), red) << "the first record's instances did not run in a multi-draw";
		EXPECT_EQ(pixelAt(multiPixels, 2, 0), green) << "the first record's instanceCount was not honoured in a multi-draw";
		EXPECT_EQ(pixelAt(multiPixels, 4, 0), blue) << "the second record's firstInstance was not honoured in a multi-draw";
		// Record 2 sits past drawCount and is the only record that would paint this slot.
		EXPECT_EQ(pixelAt(multiPixels, 6, 0), clear) << "the multi-draw ran past drawCount into the third record";
	}
}

#include "assetsModule/modelModule/Vertex.h"

namespace SFE::Render::Tests {
	// The cascade shadow pass's vertex layout, attribute for attribute: eight locations over one
	// Vertex3D binding, float attributes flagged normalized, bone ids as Int4, texCoords1 last at
	// offset 104. texCoords and texCoords1 hold different values here, and the shader puts
	// texCoords1 in red/green and texCoords in blue/alpha, so a location fed the wrong bytes shows up
	// as a named channel. The draw is shaped like Batcher's: an immutable vertex buffer bound at a
	// non-zero byte offset, 32-bit indices, two instances starting past zero, and two pipelines built
	// from one description (the pass's opaque and cutout pair) drawn back to back.
	TEST_P(RhiBackendTest, ShadowPassVertex3DLayoutDeliversEveryAttributeFromItsOwnOffset) {
		const auto target = makeTarget(rhi());
		ASSERT_TRUE(target);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 0.f });
		ASSERT_TRUE(pass);
		const auto fb = makeTargetFramebuffer(rhi(), pass, target);
		ASSERT_TRUE(fb);

		GraphicsPipelineDesc desc;
		desc.renderPass = pass;
		desc.cullMode = CullMode::None;
		desc.frontFace = FrontFace::CounterClockWise;
		desc.depthStencilState = { false, false, CompareOp::Always };
		desc.blend = { false };
		desc.layout.bindings.push_back({ 0, sizeof(Vertex3D), VertexInputRate::PerVertex });
		desc.layout.attributes.push_back({ 0, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, position)), true });
		desc.layout.attributes.push_back({ 1, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, normal)), true });
		desc.layout.attributes.push_back({ 2, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex3D, texCoords)), true });
		desc.layout.attributes.push_back({ 3, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, tangent)), true });
		desc.layout.attributes.push_back({ 4, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, biTangent)), true });
		desc.layout.attributes.push_back({ 5, 0, VertexFormat::Int4, static_cast<uint32_t>(offsetof(Vertex3D, boneIDs)), false });
		desc.layout.attributes.push_back({ 6, 0, VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex3D, weights)), false });
		desc.layout.attributes.push_back({ 7, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex3D, texCoords1)), true });
		desc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_vertex3d_layout.vs");
		desc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_vertex.fs");
		const auto opaque = rhi().createGraphicsPipeline(desc);
		const auto cutout = rhi().createGraphicsPipeline(desc);
		ASSERT_TRUE(opaque);
		ASSERT_TRUE(cutout);

		// Two decoy vertices first, skipped by the bind offset, then a triangle covering the target.
		// Every channel value is a multiple of 1/255, so the readback has no rounding to argue about.
		const float positions[5][2] = { { 0.9f, 0.9f }, { 0.9f, 0.8f }, { -1.f, -1.f }, { 3.f, -1.f }, { -1.f, 3.f } };
		std::vector<Vertex3D> verts(5);
		for (size_t i = 0; i < verts.size(); ++i) {
			Vertex3D& v = verts[i];
			v.position = { positions[i][0], positions[i][1], 0.f };
			v.texCoords = { 0.2f, 0.4f };
			v.normal = { 0.f, 0.f, 1.f };
			v.tangent = { 1.f, 0.f, 0.f };
			v.biTangent = { 0.f, 1.f, 0.f };
			v.boneIDs[0] = 3;
			v.weights[0] = 0.5f;
			v.color = { 1.f, 1.f, 1.f, 1.f };
			v.texCoords1 = { 0.8f, 0.6f };
		}
		BufferDesc vboDesc;
		vboDesc.size = sizeof(Vertex3D) * verts.size();
		vboDesc.usage = BufferUsage::Vertex;
		vboDesc.cpuAccess = CpuAccess::None;
		vboDesc.debugName = "rhi_test_vertex3d_buffer";
		const auto vbo = rhi().createBuffer(vboDesc, verts.data());
		ASSERT_TRUE(vbo);

		const uint32_t indices[3] = { 0, 1, 2 };
		BufferDesc iboDesc;
		iboDesc.size = sizeof(indices);
		iboDesc.usage = BufferUsage::Index;
		iboDesc.cpuAccess = CpuAccess::None;
		iboDesc.debugName = "rhi_test_vertex3d_indices";
		const auto ibo = rhi().createBuffer(iboDesc, indices);
		ASSERT_TRUE(ibo);

		rhi().beginFrame();
		const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
		RenderPassBeginInfo begin;
		begin.renderPass = pass;
		begin.framebuffer = fb;
		begin.width = kSize;
		begin.height = kSize;
		rhi().cmdBeginRenderPass(cmd, begin);
		for (const auto pipeline : { opaque, cutout }) {
			// Pipeline first: the OpenGL backend hangs both buffers off the bound pipeline's VAO.
			rhi().cmdBindPipeline(cmd, pipeline);
			rhi().cmdBindVertexBuffer(cmd, 0, vbo, 2 * sizeof(Vertex3D));
			rhi().cmdBindIndexBuffer(cmd, ibo, IndexType::Uint32, 0);
			rhi().cmdDrawIndexed(cmd, 3, 2, 0, 0, 5);
		}
		rhi().cmdEndRenderPass(cmd);
		rhi().endCommandList(cmd);
		rhi().submit(cmd);
		rhi().present();
		rhi().waitIdle();

		Rgba px;
		ASSERT_TRUE(readPixel(rhi(), target, 0, px));
		EXPECT_EQ(px.r, 204) << "location 7 (texCoords1.x) did not come from offsetof(Vertex3D, texCoords1)";
		EXPECT_EQ(px.g, 153) << "location 7 (texCoords1.y) did not come from offsetof(Vertex3D, texCoords1)";
		EXPECT_EQ(px.b, 51) << "location 2 (texCoords.x) did not come from offsetof(Vertex3D, texCoords)";
		EXPECT_EQ(px.a, 102) << "location 2 (texCoords.y) did not come from offsetof(Vertex3D, texCoords)";

		rhi().destroyGraphicsPipeline(opaque);
		rhi().destroyGraphicsPipeline(cutout);
		rhi().destroyBuffer(ibo);
		rhi().destroyBuffer(vbo);
		rhi().destroyFramebuffer(fb);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}

	// Which pixels a triangle owns when its edge runs exactly through pixel centres. Neither backend
	// may paint a centre on a shared edge twice or leave it empty, but which of the two triangles gets
	// it is a tie-break rule, and a Y flip done through the viewport or the winding can turn "top" into
	// "bottom". The cross-backend G-buffer diff has exactly that shape: silhouette pixels owned by a
	// different triangle on each backend.
	//
	// Everything is written in framebuffer coordinates, where row y is the y-th row of the attachment's
	// memory. Both backends map clip -1 to row 0 (OpenGL natively, Vulkan through its unflipped
	// viewport), and readbackTexture hands rows back top first, so framebuffer row y is readback row
	// 15 - y. The expectation is one map for both: the top-left rule with "top" meaning the smaller
	// row (Vulkan's framebuffer y; in OpenGL window coordinates that is the smaller window y as well,
	// i.e. the bottom of the displayed picture) and "left" meaning the smaller column.
	TEST_P(RhiBackendTest, EdgesThroughPixelCentresAreOwnedByTheSameTriangleOnBothBackends) {
		static constexpr int kFill = 16;
		struct Push {
			Math::Vec4 a{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 b{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 c{ 0.f, 0.f, 0.f, 1.f };
			Math::Vec4 color{ 1.f, 1.f, 1.f, 1.f };
		};
		// Framebuffer pixel coordinate to clip. Every value used below is a multiple of 0.5, so the
		// clip value is exact and the edge lands on the centre, not a hair beside it.
		const auto clip = [](float x, float y) {
			return Math::Vec4{ x / (kFill * 0.5f) - 1.f, y / (kFill * 0.5f) - 1.f, 0.f, 1.f };
		};
		const auto tri = [&](float ax, float ay, float bx, float by, float cx, float cy, const Math::Vec4& color) {
			Push p;
			p.a = clip(ax, ay);
			p.b = clip(bx, by);
			p.c = clip(cx, cy);
			p.color = color;
			return p;
		};

		TextureDesc texDesc;
		texDesc.width = kFill;
		texDesc.height = kFill;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		const auto target = rhi().createTexture(texDesc);
		ASSERT_TRUE(target);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 0.f });
		ASSERT_TRUE(pass);
		FramebufferDesc fbDesc;
		fbDesc.width = kFill;
		fbDesc.height = kFill;
		fbDesc.layers = 1;
		fbDesc.renderPass = pass;
		FramebufferAttachmentDesc att;
		att.texture = target;
		att.layer = UINT32_MAX;
		fbDesc.colorAttachments = { att };
		const auto fb = rhi().createFramebuffer(fbDesc);
		ASSERT_TRUE(fb);

		const auto makePipe = [&](const BlendStateDesc& blend) {
			GraphicsPipelineDesc desc;
			desc.renderPass = pass;
			desc.topology = PrimitiveTopology::Triangles;
			desc.cullMode = CullMode::None;
			desc.frontFace = FrontFace::CounterClockWise;
			desc.fillMode = PolygonFillMode::Fill;
			desc.depthStencilState = { false, false, CompareOp::Always };
			desc.blend = blend;
			desc.pushConstantSize = sizeof(Push);
			desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
			desc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_triangle.vs");
			desc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_triangle.fs");
			return rhi().createGraphicsPipeline(desc);
		};
		const auto overwrite = makePipe(BlendStateDesc{ false });
		ASSERT_TRUE(overwrite);
		const auto additive = makePipe(BlendStateDesc{ true, BlendFactor::One, BlendFactor::One });
		ASSERT_TRUE(additive);

		// One frame, every triangle in one pass, the picture read back top row first.
		const auto draw = [&](PipelineHandle pipeline, const std::vector<Push>& tris) {
			rhi().beginFrame();
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kFill;
			begin.height = kFill;
			rhi().cmdBeginRenderPass(cmd, begin);
			rhi().cmdBindPipeline(cmd, pipeline);
			for (const auto& t : tris) {
				rhi().cmdPushConstants(cmd, pipeline, ShaderStage::Vertex | ShaderStage::Fragment, &t, sizeof(t));
				rhi().cmdDraw(cmd, 3, 1, 0, 0);
			}
			rhi().cmdEndRenderPass(cmd);
			rhi().endCommandList(cmd);
			rhi().submit(cmd);
			rhi().present();
			rhi().waitIdle();
			std::vector<uint8_t> pixels;
			int w = 0;
			int h = 0;
			const bool ok = rhi().readbackTexture(target, pixels, w, h) && w == kFill && h == kFill;
			return ok ? pixels : std::vector<uint8_t>{};
		};
		// Indexed by framebuffer row: readback row = kFill - 1 - framebuffer row.
		const auto at = [&](const std::vector<uint8_t>& pixels, int x, int fbY) {
			const size_t o = (static_cast<size_t>(kFill - 1 - fbY) * kFill + static_cast<size_t>(x)) * 4u;
			return Rgba{ pixels[o], pixels[o + 1], pixels[o + 2], pixels[o + 3] };
		};
		const auto list = [](const std::vector<int>& v) {
			std::string s;
			for (const int i : v) {
				s += (s.empty() ? "" : ",") + std::to_string(i);
			}
			return "[" + s + "]";
		};
		const auto range = [](int from, int to) {
			std::vector<int> v;
			for (int i = from; i <= to; ++i) {
				v.push_back(i);
			}
			return v;
		};

		// 1. One edge at a time, on row 7 or column 7 (centre 7.5). The other two vertices sit far off
		// the target, so across the whole image the only tie is on that one row or column.
		const Math::Vec4 red{ 1.f, 0.f, 0.f, 1.f };
		struct EdgeCase {
			const char* name;
			Push push;
			bool horizontal;
			std::vector<int> expected;
		};
		const std::vector<EdgeCase> edges = {
			{ "top edge (interior at larger row)", tri(-16.f, 7.5f, 40.f, 7.5f, -16.f, 60.f, red), true, range(7, 15) },
			{ "bottom edge (interior at smaller row)", tri(-16.f, 7.5f, 40.f, 7.5f, -16.f, -45.f, red), true, range(0, 6) },
			{ "left edge (interior at larger column)", tri(7.5f, -16.f, 7.5f, 40.f, 60.f, -16.f, red), false, range(7, 15) },
			{ "right edge (interior at smaller column)", tri(7.5f, -16.f, 7.5f, 40.f, -45.f, -16.f, red), false, range(0, 6) },
		};
		for (const auto& e : edges) {
			const auto pixels = draw(overwrite, { e.push });
			ASSERT_FALSE(pixels.empty()) << e.name;
			// Every line across the edge has to agree, or "which rows" is not one answer.
			std::vector<int> covered;
			for (int i = 0; i < kFill; ++i) {
				bool any = false;
				bool all = true;
				for (int j = 0; j < kFill; ++j) {
					const bool hit = (e.horizontal ? at(pixels, j, i) : at(pixels, i, j)).r != 0;
					any = any || hit;
					all = all && hit;
				}
				EXPECT_EQ(any, all) << e.name << ": " << (e.horizontal ? "row " : "column ") << i
					<< " is only partly covered";
				if (all) {
					covered.push_back(i);
				}
			}
			EXPECT_EQ(covered, e.expected) << e.name << ": covered framebuffer "
				<< (e.horizontal ? "rows " : "columns ") << list(covered) << ", top-left rule expects "
				<< list(e.expected);
		}

		// 2. Two triangles sharing the diagonal x == y, which runs through the centre of every (i, i).
		// A (red) lies right of it, B (green) left. Under the top-left rule the diagonal is A's left edge
		// and B's right edge, so A owns it.
		const Push a = tri(-8.f, -8.f, 24.f, -8.f, 24.f, 24.f, red);
		const Push b = tri(-8.f, -8.f, 24.f, 24.f, -8.f, 24.f, { 0.f, 1.f, 0.f, 1.f });
		const Rgba kRed{ 255, 0, 0, 255 };
		const Rgba kGreen{ 0, 255, 0, 255 };

		// Overwrite: nothing may be left empty, and each side keeps its own triangle's colour.
		{
			const auto pixels = draw(overwrite, { a, b });
			ASSERT_FALSE(pixels.empty());
			for (int y = 0; y < kFill; ++y) {
				for (int x = 0; x < kFill; ++x) {
					const auto px = at(pixels, x, y);
					if (x == y) {
						EXPECT_TRUE(px == kRed || px == kGreen) << "overwrite: diagonal (" << x << "," << y << ") was missed";
					}
					else {
						EXPECT_EQ(px, x > y ? kRed : kGreen) << "overwrite: off-diagonal (" << x << "," << y << ")";
					}
				}
			}
		}

		// One/One: a pixel both triangles cover sums to yellow, one neither covers stays black.
		{
			const auto pixels = draw(additive, { a, b });
			ASSERT_FALSE(pixels.empty());
			std::vector<int> ownedByA;
			std::vector<int> ownedByB;
			std::vector<int> doubled;
			std::vector<int> missed;
			for (int y = 0; y < kFill; ++y) {
				for (int x = 0; x < kFill; ++x) {
					const auto px = at(pixels, x, y);
					if (x != y) {
						EXPECT_EQ(px, x > y ? kRed : kGreen) << "additive: off-diagonal (" << x << "," << y << ")";
						continue;
					}
					const bool r = px.r != 0;
					const bool g = px.g != 0;
					(r && g ? doubled : r ? ownedByA : g ? ownedByB : missed).push_back(x);
				}
			}
			EXPECT_TRUE(doubled.empty()) << "diagonal pixels covered by both triangles: " << list(doubled);
			EXPECT_TRUE(missed.empty()) << "diagonal pixels covered by neither triangle: " << list(missed);
			EXPECT_EQ(ownedByA, range(0, kFill - 1)) << "diagonal owned by A (right of x == y) at " << list(ownedByA)
				<< ", by B at " << list(ownedByB) << "; the top-left rule gives every one to A";
		}

		rhi().destroyGraphicsPipeline(additive);
		rhi().destroyGraphicsPipeline(overwrite);
		rhi().destroyFramebuffer(fb);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);
	}
}

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>

namespace SFE::Render::Tests {
	// What the same GLSL computes on both backends. OpenGL hands the text to the driver; Vulkan compiles
	// it to SPIR-V with shaderc and the driver compiles that. The fragment shader derives every input from
	// the pixel coordinate and the push block, runs it through the skinning chain and the helpers the
	// engine's shaders use, and writes the exact bits of one quantity per pass across RGBA8. The first
	// backend to run stores its bits; the second compares. Pass 15 writes the pixel id, so the captures
	// are matched by framebuffer pixel rather than by readback row order.
	//
	// Measured: float arithmetic is not bit-identical between the two compiler paths. Contraction of
	// multiply-add and common subexpressions are chosen differently, and marking one variable `precise`
	// moved which other quantities matched. So float quantities are held to a relative tolerance, and only
	// what is built from integer arithmetic -- sfeHash12, and selections decided by its values -- plus
	// interpolated varyings must match bit for bit. fract(sin(x) * 43758.5453), which this pass used
	// before, differed on a quarter of the pixels and is why the engine's dither now uses sfeHash12.
	TEST_P(RhiBackendTest, ShaderMathMatchesAcrossBackendsExactlyWhereItIsIntegerBased) {
		static constexpr int kBits = 64;
		static constexpr int kQuantities = 16;
		static constexpr int kIdPass = kQuantities - 1;
		static const char* const kNames[kQuantities - 1] = {
			"skinned ndc.x", "skinned ndc.y", "skinned ndc.z", "normalize(world).x", "dot(cross(n, m), world)",
			"pow", "exp", "sqrt", "mix", "smoothstep", "sfeHash12", "masked-albedo best score",
			"masked-albedo bestRgb.r", "interpolated varying.x", "interpolated varying.y",
		};
		// Captures by backend; values are indexed by framebuffer pixel (py * kBits + px).
		using Capture = std::array<std::vector<uint32_t>, kQuantities - 1>;
		static std::map<GraphicsBackend, Capture> captures;

		struct Push {
			Math::Vec4 vp0{ 1.3f, 0.f, 0.f, 0.f };
			Math::Vec4 vp1{ 0.f, 1.7f, 0.05f, 0.f };
			Math::Vec4 vp2{ 0.1f, -0.2f, -1.002f, -1.f };
			Math::Vec4 vp3{ 0.3f, -0.4f, -0.2002f, 0.5f };
			Math::Vec4 seed{ 0.0371f, -1.913f, 0.2917f, 0.00731f };
			Math::Vec4 weights{ 0.41f, 0.29f, 0.19f, 0.11f };
			Math::Vec4 uvScaleBias{ 0.731f, -0.419f, 0.123f, 0.567f };
			Math::Vec4 select{ 0.f, 0.f, 0.f, 0.f };
		};
		ASSERT_EQ(sizeof(Push), 128u);

		TextureDesc texDesc;
		texDesc.width = kBits;
		texDesc.height = kBits;
		texDesc.depth = 1;
		texDesc.arrayLayers = 1;
		texDesc.dimension = TextureDimension::Tex2D;
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		const auto target = rhi().createTexture(texDesc);
		ASSERT_TRUE(target);
		const auto pass = makeClearPass(rhi(), { 0.f, 0.f, 0.f, 0.f });
		ASSERT_TRUE(pass);
		FramebufferDesc fbDesc;
		fbDesc.width = kBits;
		fbDesc.height = kBits;
		fbDesc.layers = 1;
		fbDesc.renderPass = pass;
		FramebufferAttachmentDesc att;
		att.texture = target;
		att.layer = UINT32_MAX;
		fbDesc.colorAttachments = { att };
		const auto fb = rhi().createFramebuffer(fbDesc);
		ASSERT_TRUE(fb);

		GraphicsPipelineDesc desc;
		desc.renderPass = pass;
		desc.topology = PrimitiveTopology::Triangles;
		desc.cullMode = CullMode::None;
		desc.frontFace = FrontFace::CounterClockWise;
		desc.fillMode = PolygonFillMode::Fill;
		desc.depthStencilState = { false, false, CompareOp::Always };
		desc.blend = { false };
		desc.pushConstantSize = sizeof(Push);
		desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		desc.shaders.vertex = rhi().loadShader(ShaderStage::Vertex, "shaders/rhi_test_float_bits.vs");
		desc.shaders.fragment = rhi().loadShader(ShaderStage::Fragment, "shaders/rhi_test_float_bits.fs");
		const auto pipeline = rhi().createGraphicsPipeline(desc);
		ASSERT_TRUE(pipeline);

		// One frame, one full-screen draw writing quantity q; returns the uint per readback pixel.
		const auto drawQuantity = [&](int q) {
			Push push;
			push.select.x = static_cast<float>(q);
			rhi().beginFrame();
			const auto cmd = rhi().beginCommandList(CommandListType::Graphics);
			RenderPassBeginInfo begin;
			begin.renderPass = pass;
			begin.framebuffer = fb;
			begin.width = kBits;
			begin.height = kBits;
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
			std::vector<uint32_t> out;
			if (!rhi().readbackTexture(target, pixels, w, h) || w != kBits || h != kBits || pixels.size() < kBits * kBits * 4u) {
				return out;
			}
			out.resize(kBits * kBits);
			for (size_t i = 0; i < out.size(); ++i) {
				out[i] = uint32_t(pixels[i * 4]) | uint32_t(pixels[i * 4 + 1]) << 8 | uint32_t(pixels[i * 4 + 2]) << 16
					| uint32_t(pixels[i * 4 + 3]) << 24;
			}
			return out;
		};

		// Map each readback pixel to the framebuffer pixel that wrote it.
		const auto ids = drawQuantity(kIdPass);
		ASSERT_EQ(ids.size(), size_t(kBits * kBits)) << "id pass readback failed";
		std::vector<bool> seen(kBits * kBits, false);
		for (const uint32_t id : ids) {
			const uint32_t px = id & 0xFFFFu;
			const uint32_t py = id >> 16;
			ASSERT_TRUE(px < kBits && py < kBits && !seen[py * kBits + px]) << "id pass is not a permutation of the pixels";
			seen[py * kBits + px] = true;
		}

		Capture capture;
		for (int q = 0; q < kIdPass; ++q) {
			const auto raw = drawQuantity(q);
			ASSERT_EQ(raw.size(), size_t(kBits * kBits)) << kNames[q] << ": readback failed";
			capture[q].resize(raw.size());
			for (size_t i = 0; i < raw.size(); ++i) {
				capture[q][(ids[i] >> 16) * kBits + (ids[i] & 0xFFFFu)] = raw[i];
			}
		}
		captures[GetParam()] = capture;

		rhi().destroyGraphicsPipeline(pipeline);
		rhi().destroyFramebuffer(fb);
		rhi().destroyRenderPass(pass);
		rhi().destroyTexture(target);

		// Nothing to compare until both backends have run in this process.
		if (captures.size() < 2) {
			return;
		}
		const Capture& gl = captures.at(GraphicsBackend::OpenGL);
		const Capture& vk = captures.at(GraphicsBackend::Vulkan);
		const auto toFloat = [](uint32_t b) {
			float f = 0.f;
			std::memcpy(&f, &b, sizeof(f));
			return f;
		};
		for (int q = 0; q < kIdPass; ++q) {
			int differing = 0;
			double maxRel = 0.0;
			int sample = -1;
			for (int i = 0; i < kBits * kBits; ++i) {
				if (gl[q][i] == vk[q][i]) {
					continue;
				}
				++differing;
				const double a = toFloat(gl[q][i]);
				const double b = toFloat(vk[q][i]);
				const double scale = std::max(std::abs(a), std::abs(b));
				const double rel = std::isfinite(a) && std::isfinite(b) && scale > 0.0
					? std::abs(a - b) / scale : std::numeric_limits<double>::infinity();
				if (sample < 0 || rel > maxRel) {
					maxRel = rel;
					sample = i;
				}
			}
			char detail[256] = "";
			if (sample >= 0) {
				std::snprintf(detail, sizeof(detail), "; max relative difference %.3g at pixel (%d,%d): OpenGL %.9g (0x%08X), Vulkan %.9g (0x%08X)",
					maxRel, sample % kBits, sample / kBits, toFloat(gl[q][sample]), gl[q][sample], toFloat(vk[q][sample]), vk[q][sample]);
			}
			const bool mustBeExact = q >= 10;
			if (mustBeExact) {
				EXPECT_EQ(differing, 0) << kNames[q] << ": " << differing << " of " << kBits * kBits << " pixels differ in bits" << detail;
			}
			else {
				int outside = 0;
				for (int i = 0; i < kBits * kBits; ++i) {
					const double a = toFloat(gl[q][i]);
					const double b = toFloat(vk[q][i]);
					if (std::abs(a - b) > 1e-4 * std::max({ std::abs(a), std::abs(b), 1.0 })) {
						++outside;
					}
				}
				EXPECT_EQ(outside, 0) << kNames[q] << ": " << outside << " pixels beyond the float tolerance" << detail;
				if (differing > 0) {
					std::printf("[ INFO     ] %s: %d of %d pixels differ in bits, within tolerance%s\n", kNames[q], differing, kBits * kBits, detail);
				}
			}
		}
	}
}
