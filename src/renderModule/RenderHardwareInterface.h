#pragma once
#include <optional>
#include <vector>

#include "mathModule/Forward.h"

namespace SFE::Render
{
#define RHI_INVALID_HANDLE UINT32_MAX

	// Handles
	struct DescriptorSetLayoutHandle
	{
		uint32_t id = RHI_INVALID_HANDLE;
		operator bool() const { return id != RHI_INVALID_HANDLE; }
	};

	struct DescriptorSetHandle
	{
		uint32_t id = RHI_INVALID_HANDLE;
		operator bool() const { return id != RHI_INVALID_HANDLE; }
	};

	struct TextureHandle
	{
		uint32_t id = RHI_INVALID_HANDLE;

		operator bool() const { return id != RHI_INVALID_HANDLE; }
	};

	struct SamplerHandle {
		uint32_t id = RHI_INVALID_HANDLE;

		operator bool() const { return id != RHI_INVALID_HANDLE; }
	};

	struct ShaderModuleHandle
	{
		uint32_t id = RHI_INVALID_HANDLE;

		operator bool() const { return id != RHI_INVALID_HANDLE; }
	};

	enum class IndexType
	{
		Uint16,
		Uint32
	};

	struct BufferHandle
	{
		uint32_t id = RHI_INVALID_HANDLE;

		operator bool() const { return id != RHI_INVALID_HANDLE; }

		void bindVertex(uint32_t slot = 0, uint32_t offset = 0) const;
		void bindIndex(IndexType type, uint32_t offset = 0) const;
		void bindUniform(uint32_t binding) const;
		void bindStorage(uint32_t binding) const;
	};

	struct RenderPassHandle {
		uint32_t id = RHI_INVALID_HANDLE;

		operator bool() const { return id != RHI_INVALID_HANDLE; }
	};

	struct PipelineHandle {
		uint32_t id = RHI_INVALID_HANDLE;

		operator bool() const { return id != RHI_INVALID_HANDLE; }
		void bind() const;
	};

	// Descriptions

	enum class ShaderStage : uint32_t {
		Vertex = 1 << 0,
		Fragment = 1 << 1,
		Geometry = 1 << 2,
		TessControl = 1 << 3,
		TessEval = 1 << 4,
		Compute = 1 << 5,
	};

	struct ShaderModuleDesc
	{
		ShaderStage stage;
		std::string name;     // logic name
		std::string source;   // GLSL/HLSL text or SPIR-V bytes as string
	};

	enum class TextureDimension { Tex2D, Tex3D, Array, Cube };

	enum class TextureFormat
	{
		R8,
		R8_SNORM,
		R16,
		R16_SNORM,
		RG8,
		RG8_SNORM,
		RG16,
		RG16_SNORM,
		R3_G3_B2,
		RGB4,
		RGB5,
		RGB8,
		RGB8_SNORM,
		RGB10,
		RGB12,
		RGB16_SNORM,
		RGBA2,
		RGBA4,
		RGB5_A1,
		RGBA8,
		RGBA8_SNORM,
		RGB10_A2,
		RGB10_A2UI,
		RGBA12,
		RGBA16,
		SRGB8,
		SRGB8_ALPHA8,
		R16F,
		RG16F,
		RGB16F,
		RGBA16F,
		R32F,
		RG32F,
		RGB32F,
		RGBA32F,
		R11F_G11F_B10F,
		RGB9_E5,
		R8I,
		R8UI,
		R16I,
		R16UI,
		R32I,
		R32UI,
		RG8I,
		RG8UI ,
		RG16I,
		RG16UI,
		RG32I,
		RG32UI,
		RGB8I,
		RGB8UI,
		RGB16I,
		RGB16UI,
		RGB32I,
		RGB32UI,
		RGBA8I,
		RGBA8UI,
		RGBA16I,
		RGBA16UI,
		RGBA32I,
		RGBA32UI,
		DEPTH_COMPONENT32F,
		DEPTH_COMPONENT16,
		DEPTH_COMPONENT24,
		DEPTH_COMPONENT32,
		DEPTH24_STENCIL8,
		DEPTH32F_STENCIL8,
	};

	enum class TextureUsage : uint32_t {
		None = 0,
		Sampled = 1 << 0,
		ColorAttachment = 1 << 1,
		DepthStencilAttachment = 1 << 2,
		TransferSrc = 1 << 3,
		TransferDst = 1 << 4,
	};

	inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
		return TextureUsage(uint32_t(a) | uint32_t(b));
	}

	enum class FilterMode { Nearest, Linear };
	enum class AddressMode { Repeat, ClampToEdge, ClampToBorder, MirrorRepeat, MirrorClampToEdge};

	enum class CompareOp {
		None,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,
		NotEqual,
		Always,
		Never,
	};

	struct TextureDesc {
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 1;
		uint32_t mipLevels = 1;
		uint32_t arrayLayers = 1;

		TextureDimension dimension = TextureDimension::Tex2D;
		TextureFormat    format = TextureFormat::RGBA8;
		TextureUsage     usage = TextureUsage::Sampled;
	};

	struct SamplerDesc {
		FilterMode  minFilter = FilterMode::Linear;
		FilterMode  magFilter = FilterMode::Linear;
		AddressMode addressU = AddressMode::ClampToEdge; //wrap.S
		AddressMode addressV = AddressMode::ClampToEdge; //wrap.T
		AddressMode addressW = AddressMode::ClampToEdge; //wrap.R
		CompareOp   compareOp = CompareOp::None;
	};

	struct FramebufferHandle { uint32_t id = UINT32_MAX; };

	struct FramebufferAttachmentDesc {
		TextureHandle		texture;
		uint32_t            mipLevel = 0;
		uint32_t            layer = 0;   // для 2D-array / 3D / cubemap
	};

	struct FramebufferDesc {
		uint32_t width = 0;
		uint32_t height = 0;

		std::vector<FramebufferAttachmentDesc> colorAttachments;
		std::optional<FramebufferAttachmentDesc> depthStencilAttachment;
	};

	struct ViewportDesc {
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};

	enum class BufferUsage
	{
		Vertex,
		Index,
		Uniform,   // UBO
		Storage,   // SSBO
		Indirect
	};

	enum class CpuAccess
	{
		None,       // GPU-only
		Write,      // CPU → GPU
		Read,       // GPU → CPU
		ReadWrite
	};

	enum class MapMode
	{
		Read,
		Write,
		ReadWrite
	};

	struct BufferDesc
	{
		size_t size = 0;
		BufferUsage usage = BufferUsage::Vertex;
		CpuAccess cpuAccess = CpuAccess::Write;
		const char* debugName = nullptr;
	};

	enum class VertexFormat
	{
		Float,      // 1 * 32
		Float2,
		Float3,
		Float4,
		Int,
		Int2,
		Int3,
		Int4,
		UInt,
		UInt2,
		UInt3,
		UInt4
		// при желании – Uint, Uint2, ...
	};

	enum class VertexInputRate : uint8_t
	{
		PerVertex,
		PerInstance
	};

	struct VertexAttributeDesc
	{
		std::uint32_t location = 0;
		std::uint32_t binding = 0;        // номер биндинга (0,1,2...) – как в Vulkan
		VertexFormat  format = VertexFormat::Float3;
		std::uint32_t offset = 0;
		bool          normalized = false; // нужно для OpenGL backend
	};

	struct VertexBindingDesc
	{
		std::uint32_t binding = 0;        // индекс биндинга
		std::uint32_t stride = 0;         // sizeof(Vertex)
		VertexInputRate inputRate = VertexInputRate::PerVertex;
	};

	struct VertexLayoutDesc
	{
		std::vector<VertexBindingDesc>   bindings;
		std::vector<VertexAttributeDesc> attributes;
	};

	enum class BlitMask : uint32_t
	{
		Color = 0x1,
		Depth = 0x2,
		Stencil = 0x4
	};

	enum class BlitFilter
	{
		Nearest,
		Linear
	};



	// --- RenderPass ---

	enum class AttachmentLoadOp {
		Load,
		Clear,
		DontCare
	};

	enum class AttachmentStoreOp {
		Store,
		DontCare
	};

	struct RenderPassColorAttachmentDesc {
		AttachmentLoadOp  loadOp = AttachmentLoadOp::Clear;
		AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
		Math::Vec4        clearColor{ 0.f, 0.f, 0.f, 1.f };
	};

	struct RenderPassDepthAttachmentDesc {
		AttachmentLoadOp  loadOp = AttachmentLoadOp::Clear;
		AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
		float             clearDepth = 1.f;
		uint8_t           clearStencil = 0;
		bool              hasStencil = false;
	};

	struct RenderPassDesc {
		uint32_t colorAttachmentCount = 0;
		RenderPassColorAttachmentDesc colorAttachments[8];
		bool                          hasDepthStencil = false;
		RenderPassDepthAttachmentDesc depthAttachment;
	};

	// begin info — чем реально запускаем pass
	struct RenderPassBeginInfo {
		RenderPassHandle   renderPass;
		FramebufferHandle  framebuffer;
		int                x = 0, y = 0;
		int                width = 0, height = 0;

		// опционально можем переопределить clear-значения
		const Math::Vec4* overrideClearColors = nullptr; // size >= colorAttachmentCount
		const float* overrideClearDepth = nullptr;
		const uint8_t* overrideClearStencil = nullptr;
	};

	// --- Pipeline ---

	enum class PrimitiveTopology {
		Triangles,
		Lines,
		Points
	};

	enum class CullMode {
		None,
		Front,
		Back
	};

	enum class FrontFace {
		ClockWise,
		CounterClockWise
	};

	enum class PolygonFillMode {
		Fill,
		Line
	};

	struct DepthStencilStateDesc {
		bool      depthTest = true;
		bool      depthWrite = true;
		CompareOp depthCompareOp = CompareOp::Less; // твой уже существующий enum
	};

	enum class BlendFactor {
		One,
		Zero,
		SrcAlpha,
		OneMinusSrcAlpha
	};

	struct BlendStateDesc {
		bool       enabled = false;
		BlendFactor srcColor = BlendFactor::SrcAlpha;
		BlendFactor dstColor = BlendFactor::OneMinusSrcAlpha;
	};

	struct PipelineShaders
	{
		ShaderModuleHandle vertex;
		ShaderModuleHandle fragment;
		ShaderModuleHandle geometry;     // optional
		ShaderModuleHandle tessControl;  // optional
		ShaderModuleHandle tessEval;     // optional
		// compute separately in ComputePipelineDesc
	};

	struct GraphicsPipelineDesc {
		PipelineShaders shaders;

		VertexLayoutDesc		layout;

		PrimitiveTopology		topology = PrimitiveTopology::Triangles;
		CullMode				cullMode = CullMode::Back;
		FrontFace				frontFace = FrontFace::CounterClockWise;
		PolygonFillMode			fillMode = PolygonFillMode::Fill;
		DepthStencilStateDesc	depthStencilState;
		BlendStateDesc			blend;

		// Для Vulkan backend: с каким render pass и субпассом совместим этот pipeline.
		// Для OpenGL backend может игнорироваться.
		RenderPassHandle		renderPass = {};
		uint32_t				subpassIndex = 0; // пока всегда 0, т.к. субпассов нет
	};

	// --- Command lists ---

	enum class CommandListType
	{
		Graphics,
		Compute,
		Transfer
	};

	struct CommandListHandle
	{
		uint32_t id = UINT32_MAX;

		operator bool() const { return id != UINT32_MAX; }
	};


	inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
		return ShaderStage(uint32_t(a) | uint32_t(b));
	}

	inline bool operator&(ShaderStage a, ShaderStage b) {
		return (uint32_t(a) & uint32_t(b)) != 0;
	}

	enum class DescriptorType
	{
		UniformBuffer,
		StorageBuffer,
		CombinedImageSampler, // текстура + сэмплер
	};

	struct DescriptorBufferInfo
	{
		BufferHandle buffer;
		size_t       offset = 0;
		size_t       range = 0; // 0 = "до конца буфера"
	};

	struct DescriptorImageInfo
	{
		TextureHandle  texture;
		SamplerHandle  sampler;
	};

	struct WriteDescriptor
	{
		DescriptorSetHandle set; // в какой set пишем
		uint32_t            binding = 0; // binding в layout’е / шейдере
		DescriptorType      type;

		std::vector<DescriptorBufferInfo> buffers;
		std::vector<DescriptorImageInfo>  images;
	};

	struct DescriptorSetLayoutBinding
	{
		uint32_t      binding = 0;
		DescriptorType type;
		uint32_t      count = 1;          // для массивов (texture[4] и т.п.)
		ShaderStage   stageMask;          // какие шейдер-стадии видят этот binding
	};

	struct DescriptorSetLayoutDesc
	{
		std::vector<DescriptorSetLayoutBinding> bindings;
	};

	class RenderHardwareInterface
	{
	public:
		virtual ~RenderHardwareInterface() = default;

		virtual void init() = 0;
		virtual void cleanup() = 0;

		virtual PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		virtual void bindPipeline(PipelineHandle handle) = 0;

		virtual TextureHandle createTexture(const TextureDesc& desc, const void* data = nullptr) = 0;
		virtual void destroyTexture(TextureHandle texture) = 0;

		virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;
		virtual void destroySampler(SamplerHandle sampler) = 0;

		virtual void bindTexture(uint32_t slot, TextureHandle tex, SamplerHandle sampler = {}) = 0;
		virtual void setTextureSubImage2D(TextureHandle tex, uint32_t x, uint32_t y, uint32_t w, uint32_t h, TextureFormat fmt, const void* data) = 0;
		
		virtual ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) = 0;
		virtual void destroyShaderModule(ShaderModuleHandle handle) = 0;

		virtual FramebufferHandle createFramebuffer(const FramebufferDesc& desc) = 0;
		virtual void destroyFramebuffer(FramebufferHandle framebuffer) = 0;
		virtual void bindFramebuffer(FramebufferHandle framebuffer) = 0;
		virtual void blitFramebuffer(
			FramebufferHandle src,
			FramebufferHandle dst,
			int srcX0, int srcY0, int srcX1, int srcY1,
			int dstX0, int dstY0, int dstX1, int dstY1,
			BlitMask mask,
			BlitFilter filter) = 0;

		virtual BufferHandle createBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
		virtual void destroyBuffer(BufferHandle handle) = 0;
		template<typename T>
		void updateBuffer(BufferHandle handle, const T& data, size_t offset = 0) {
			updateBuffer(handle, offset, sizeof(T), static_cast<const void*>(&data));
		}

		virtual void updateBuffer(BufferHandle handle, size_t offset = 0, size_t size = 0, const void* data = nullptr) = 0;

		virtual void* mapBuffer(BufferHandle handle, MapMode mode) = 0;
		virtual void  unmapBuffer(BufferHandle handle) = 0;

		virtual void bindVertexBuffer(uint32_t slot, BufferHandle handle, uint32_t offset = 0) = 0;
		virtual void bindIndexBuffer(BufferHandle handle, IndexType type, uint32_t offset = 0) = 0;
		virtual void bindUniformBuffer(uint32_t binding, BufferHandle handle) = 0;
		virtual void bindStorageBuffer(uint32_t binding, BufferHandle handle) = 0;


		virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0, uint32_t instanceCount = 1) = 0;
		virtual void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t instanceCount = 1, IndexType indexType = IndexType::Uint32, int32_t  vertexOffset = 0) = 0;

		virtual void setUniform(PipelineHandle handle, const std::string& name, bool value) = 0;
		virtual void setUniform(PipelineHandle handle, const std::string& name, int value) = 0;
		virtual void setUniform(PipelineHandle handle, const std::string& name, float value) = 0;
		virtual void setUniform(PipelineHandle handle, const std::string& name, const Math::Mat4& value) = 0;
		virtual void setUniform(PipelineHandle handle, const std::string& name, const Math::Vec4& value) = 0;
		virtual void setUniform(PipelineHandle handle, const std::string& name, const Math::Vec3& value) = 0;
		virtual void setUniform(PipelineHandle handle, const std::string& name, const Math::Vec2& value) = 0;

		// --- RenderPass ---
		virtual RenderPassHandle createRenderPass(const RenderPassDesc& desc) = 0;
		virtual void destroyRenderPass(RenderPassHandle renderPass) = 0;

		// begin/end — "правильный" путь рисовать: сначала beginRenderPass, потом draw, потом endRenderPass.
		virtual void beginRenderPass(const RenderPassBeginInfo& beginInfo) = 0;
		virtual void endRenderPass() = 0;

		// --- Command lists (новое, для постепенного перехода) ---

		virtual CommandListHandle beginCommandList(CommandListType type) = 0;
		virtual void endCommandList(CommandListHandle cmd) = 0;
		virtual void submit(const CommandListHandle* lists, uint32_t count) = 0;
		virtual void present() = 0;

		virtual void cmdBeginRenderPass(CommandListHandle cmd, const RenderPassBeginInfo& beginInfo) = 0;
		virtual void cmdEndRenderPass(CommandListHandle cmd) = 0;

		virtual void cmdBindPipeline(CommandListHandle cmd, PipelineHandle pipeline) = 0;

		virtual void cmdBindVertexBuffer(CommandListHandle cmd, uint32_t slot, BufferHandle handle, uint32_t offset = 0) = 0;
		virtual void cmdBindIndexBuffer(CommandListHandle cmd, BufferHandle handle, IndexType type, uint32_t offset = 0) = 0;

		template<typename T>
		void cmdPushConstants(CommandListHandle cmd, PipelineHandle pipeline, const T& data, ShaderStage stages = ShaderStage::Vertex | ShaderStage::Fragment) {
			cmdPushConstants(cmd, pipeline, stages, static_cast<const void*>(&data), static_cast<uint32_t>(sizeof(T)));
		}

		virtual void cmdPushConstants(
			CommandListHandle cmd,
			PipelineHandle pipeline,
			ShaderStage stages = ShaderStage::Vertex | ShaderStage::Fragment,
			const void* data = nullptr,
			uint32_t size = 0
		) = 0;

		virtual void cmdDraw(CommandListHandle cmd,
			uint32_t vertexCount,
			uint32_t instanceCount = 1,
			uint32_t firstVertex = 0,
			uint32_t firstInstance = 0) = 0;

		virtual void cmdDrawIndexed(CommandListHandle cmd,
			uint32_t indexCount,
			uint32_t instanceCount = 1,
			uint32_t firstIndex = 0,
			int32_t  vertexOffset = 0,
			uint32_t firstInstance = 0) = 0;

		// --- Descriptor sets / layouts ---
		virtual DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;

		virtual void destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout) = 0;

		virtual DescriptorSetHandle allocateDescriptorSet(DescriptorSetLayoutHandle layout) = 0;
		virtual void freeDescriptorSet(DescriptorSetHandle set) = 0;

		virtual void updateDescriptorSets(const std::vector<WriteDescriptor>& writes) = 0;

		virtual void cmdBindDescriptorSets(CommandListHandle cmd, PipelineHandle layout, uint32_t firstSet, const DescriptorSetHandle* sets, uint32_t setCount) = 0;
	};
}
