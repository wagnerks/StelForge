#pragma once
#include <optional>
#include <vector>

#include "mathModule/Forward.h"

namespace SFE::Render
{
#define IS_VALID_FUNC operator bool() const { return id != UINT32_MAX; }

	// Handles
	struct DescriptorSetLayoutHandle	{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct DescriptorSetHandle			{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct TextureHandle				{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct SamplerHandle				{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct ShaderModuleHandle			{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct BufferHandle					{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct RenderPassHandle				{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct PipelineHandle				{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct FramebufferHandle			{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };
	struct CommandListHandle			{ uint32_t id = UINT32_MAX; IS_VALID_FUNC };

	// Enums
	enum class IndexType : uint8_t { Uint16, Uint32 };

	enum class ShaderStage : uint8_t { Vertex = 1 << 0, Fragment = 1 << 1, Geometry = 1 << 2, TessControl = 1 << 3, TessEval = 1 << 4, Compute = 1 << 5, };

	inline ShaderStage operator|(ShaderStage a, ShaderStage b) { return static_cast<ShaderStage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
	inline bool operator&(ShaderStage a, ShaderStage b) { return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0; }

	enum class TextureDimension : uint8_t { Tex2D, Tex3D, Array, Cube };

	enum class TextureFormat : uint8_t {
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
		RG8UI,
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

	enum class TextureUsage : uint8_t {
		None = 0,
		Sampled = 1 << 0,
		ColorAttachment = 1 << 1,
		DepthStencilAttachment = 1 << 2,
		TransferSrc = 1 << 3,
		TransferDst = 1 << 4,
	};

	inline TextureUsage operator|(TextureUsage a, TextureUsage b) { return static_cast<TextureUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }

	enum class FilterMode : uint8_t { Nearest, Linear };

	enum class AddressMode : uint8_t { Repeat, ClampToEdge, ClampToBorder, MirrorRepeat, MirrorClampToEdge };

	enum class BufferUsage : uint8_t { Vertex, Index, Uniform, Storage, Indirect };

	enum class CpuAccess : uint8_t {
		None,       // GPU-only
		Write,      // CPU → GPU
		Read,       // GPU → CPU
		ReadWrite
	};

	enum class MapMode : uint8_t { Read, Write, ReadWrite };

	enum class VertexFormat : uint8_t {
		Float,
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
	};

	enum class CompareOp : uint8_t {
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

	enum class VertexInputRate : uint8_t { PerVertex, PerInstance };

	enum class BlitMask : uint8_t { Color = 0x1, Depth = 0x2, Stencil = 0x4 };

	inline BlitMask operator|(BlitMask a, BlitMask b) { return static_cast<BlitMask>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
	inline bool operator&(BlitMask a, BlitMask b) { return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0; }

	enum class BlitFilter : uint8_t { Nearest, Linear };

	enum class AttachmentLoadOp : uint8_t { Load, Clear, DontCare };

	enum class AttachmentStoreOp : uint8_t { Store, DontCare };

	enum class PrimitiveTopology : uint8_t { Triangles, TriangleStrip, Lines, Points, LineLoop };

	enum class CullMode : uint8_t { None, Front, Back };

	enum class FrontFace : uint8_t { ClockWise, CounterClockWise };

	enum class PolygonFillMode : uint8_t { Fill, Line };

	enum class BlendFactor : uint8_t { One, Zero, SrcAlpha, OneMinusSrcAlpha };
	
	enum class CommandListType : uint8_t { Graphics, Compute, Transfer };

	enum class PipelineStage : uint32_t {
		None = 0,
		DrawIndirect = 1u << 0,
		VertexInput = 1u << 1,
		VertexShader = 1u << 2,
		FragmentShader = 1u << 3,
		ColorAttachmentOutput = 1u << 4,
		LateFragmentTests = 1u << 5,
		Transfer = 1u << 6,
		Host = 1u << 7,
		AllGraphics = 1u << 8,
		AllCommands = 1u << 9,
	};

	inline PipelineStage operator|(PipelineStage a, PipelineStage b) {
		return static_cast<PipelineStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}
	inline bool operator&(PipelineStage a, PipelineStage b) {
		return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
	}

	enum class AccessMask : uint32_t {
		None = 0,
		IndirectCommandRead = 1u << 0,
		IndexRead = 1u << 1,
		VertexAttributeRead = 1u << 2,
		UniformRead = 1u << 3,
		ShaderRead = 1u << 4,
		ShaderWrite = 1u << 5,
		ColorAttachmentRead = 1u << 6,
		ColorAttachmentWrite = 1u << 7,
		DepthStencilAttachmentRead = 1u << 8,
		DepthStencilAttachmentWrite = 1u << 9,
		TransferRead = 1u << 10,
		TransferWrite = 1u << 11,
		HostRead = 1u << 12,
		HostWrite = 1u << 13,
	};

	inline AccessMask operator|(AccessMask a, AccessMask b) {
		return static_cast<AccessMask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}
	inline bool operator&(AccessMask a, AccessMask b) {
		return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
	}

	struct PipelineBarrier {
		PipelineStage srcStage = PipelineStage::AllCommands;
		PipelineStage dstStage = PipelineStage::AllCommands;
		AccessMask srcAccess = AccessMask::None;
		AccessMask dstAccess = AccessMask::None;
	};

	enum class DescriptorType : uint8_t { UniformBuffer, StorageBuffer,	CombinedImageSampler, };

	// Descriptions
	struct ShaderModuleDesc {
		ShaderStage stage;
		std::string name;     // logic name
		std::string source;   // GLSL/HLSL text or SPIR-V bytes as string
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

	struct FramebufferAttachmentDesc {
		TextureHandle		texture;
		uint32_t            mipLevel = 0;
		uint32_t            layer = 0;   // for 2D-array / 3D / cubemap; UINT32_MAX = all layers (gl_Layer)
	};

	struct FramebufferDesc {
		uint32_t width = 0;
		uint32_t height = 0;

		std::vector<FramebufferAttachmentDesc> colorAttachments;
		std::optional<FramebufferAttachmentDesc> depthStencilAttachment;
	};

	struct BufferDesc {
		size_t size = 0;
		BufferUsage usage = BufferUsage::Vertex;
		CpuAccess cpuAccess = CpuAccess::Write;
		const char* debugName = nullptr;
	};

	struct VertexAttributeDesc {
		std::uint32_t location = 0;
		std::uint32_t binding = 0;
		VertexFormat  format = VertexFormat::Float3;
		std::uint32_t offset = 0;
		bool          normalized = false; // for openGL only
	};

	struct VertexBindingDesc {
		std::uint32_t binding = 0;   
		std::uint32_t stride = 0; // sizeof(Vertex)
		VertexInputRate inputRate = VertexInputRate::PerVertex;
	};

	struct VertexLayoutDesc {
		std::vector<VertexBindingDesc>   bindings;
		std::vector<VertexAttributeDesc> attributes;
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
		uint32_t						colorAttachmentCount = 0;
		RenderPassColorAttachmentDesc	colorAttachments[8];
		bool							hasDepthStencil = false;
		RenderPassDepthAttachmentDesc	depthAttachment;
	};

	struct RenderPassBeginInfo {
		RenderPassHandle   renderPass;
		FramebufferHandle  framebuffer;
		int                x = 0, y = 0;
		int                width = 0, height = 0;

		const Math::Vec4* overrideClearColors = nullptr; // size >= colorAttachmentCount
		const float* overrideClearDepth = nullptr;
		const uint8_t* overrideClearStencil = nullptr;
	};

	struct DepthStencilStateDesc {
		bool      depthTest = true;
		bool      depthWrite = true;
		CompareOp depthCompareOp = CompareOp::Less;
	};

	struct BlendStateDesc {
		bool       enabled = false;
		BlendFactor srcColor = BlendFactor::SrcAlpha;
		BlendFactor dstColor = BlendFactor::OneMinusSrcAlpha;
	};

	struct PipelineShaders {
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

		// For openGL backend it can be ignored.
		// For Vulkan backend: which render pass and subpass this pipeline is compatible with.
		RenderPassHandle		renderPass = {};
		uint32_t				subpassIndex = 0; // meanwhile only 0
	};

	struct DescriptorBufferInfo {
		BufferHandle buffer;
		size_t       offset = 0;
		size_t       range = 0; // 0 - whole buffer
	};

	struct DescriptorImageInfo {
		TextureHandle  texture;
		SamplerHandle  sampler;
	};

	struct WriteDescriptor {
		DescriptorSetHandle set; // in which set to write
		uint32_t            binding = 0; // binding in shader layout
		DescriptorType      type;

		std::vector<DescriptorBufferInfo> buffers;
		std::vector<DescriptorImageInfo>  images;
	};

	struct DescriptorSetLayoutBinding {
		uint32_t		binding = 0;
		DescriptorType	type;
		ShaderStage		stageMask;          // which shader stages will access this binding
		uint32_t		count = 1;          // for texture arrays
	};

	struct DescriptorSetLayoutDesc {
		std::vector<DescriptorSetLayoutBinding> bindings;
	};

	class RenderHardwareInterface {
	public:
		RenderHardwareInterface(const RenderHardwareInterface& other) = delete;
		RenderHardwareInterface(RenderHardwareInterface&& other) noexcept = delete;
		RenderHardwareInterface& operator=(const RenderHardwareInterface& other) = delete;
		RenderHardwareInterface& operator=(RenderHardwareInterface&& other) noexcept = delete;

		RenderHardwareInterface() = default;
		virtual ~RenderHardwareInterface() = default;

		virtual void init() = 0;
		virtual void cleanup() = 0;

		virtual PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		virtual void destroyGraphicsPipeline(PipelineHandle pipeline) = 0;
		virtual TextureHandle createTexture(const TextureDesc& desc, const void* data = nullptr) = 0;
		virtual void destroyTexture(TextureHandle texture) = 0;

		virtual SamplerHandle createSampler(const SamplerDesc& desc) = 0;
		virtual void destroySampler(SamplerHandle sampler) = 0;

		virtual ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) = 0;
		virtual void destroyShaderModule(ShaderModuleHandle handle) = 0;

		virtual FramebufferHandle createFramebuffer(const FramebufferDesc& desc) = 0;
		virtual void destroyFramebuffer(FramebufferHandle framebuffer) = 0;
		
		virtual FramebufferHandle getBackbufferFramebuffer() = 0;

		virtual BufferHandle createBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
		virtual void destroyBuffer(BufferHandle handle) = 0;
		virtual void updateBuffer(BufferHandle handle, size_t offset = 0, size_t size = 0, const void* data = nullptr) = 0;
		
		virtual void* mapBuffer(BufferHandle handle, MapMode mode) = 0;
		virtual void  unmapBuffer(BufferHandle handle) = 0;

		virtual RenderPassHandle createRenderPass(const RenderPassDesc& desc) = 0;
		virtual void destroyRenderPass(RenderPassHandle renderPass) = 0;

		virtual DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;
		virtual void destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout) = 0;

		virtual DescriptorSetHandle allocateDescriptorSet(DescriptorSetLayoutHandle layout) = 0;
		virtual void freeDescriptorSet(DescriptorSetHandle set) = 0;

		virtual void updateDescriptorSets(const std::vector<WriteDescriptor>& writes) = 0;

		virtual CommandListHandle beginCommandList(CommandListType type) = 0;
		virtual void endCommandList(CommandListHandle cmd) = 0;

		virtual void cmdBeginRenderPass(CommandListHandle cmd, const RenderPassBeginInfo& beginInfo) = 0;
		virtual void cmdEndRenderPass(CommandListHandle cmd) = 0;

		virtual void cmdBindPipeline(CommandListHandle cmd, PipelineHandle pipeline) = 0;

		virtual void cmdBindVertexBuffer(CommandListHandle cmd, uint32_t slot, BufferHandle handle, uint32_t offset = 0) = 0;
		virtual void cmdBindIndexBuffer(CommandListHandle cmd, BufferHandle handle, IndexType type, uint32_t offset = 0) = 0;
		virtual void cmdBindDescriptorSets(CommandListHandle cmd, PipelineHandle layout, uint32_t firstSet, const DescriptorSetHandle* sets, uint32_t setCount) = 0;
		virtual void cmdPushConstants(CommandListHandle cmd, PipelineHandle pipeline, ShaderStage stages = ShaderStage::Vertex | ShaderStage::Fragment, const void* data = nullptr, uint32_t size = 0) = 0;

		virtual void cmdDraw(CommandListHandle cmd, uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
		virtual void cmdDrawIndexed(CommandListHandle cmd, uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t  vertexOffset = 0, uint32_t firstInstance = 0) = 0;

		virtual void cmdBlit(CommandListHandle cmd, FramebufferHandle src, FramebufferHandle dst, BlitMask mask = BlitMask::Color, BlitFilter filter = BlitFilter::Nearest) = 0;
		virtual void cmdPipelineBarrier(CommandListHandle cmd, const PipelineBarrier& barrier) = 0;
		virtual void cmdUpdateBuffer(CommandListHandle cmd, BufferHandle handle, size_t offset, size_t size, const void* data) = 0;

		virtual void submit(const CommandListHandle* lists, uint32_t count) = 0;
		virtual void present() = 0;

		virtual void updateTexture(TextureHandle tex, uint32_t x, uint32_t y, uint32_t w, uint32_t h, TextureFormat fmt, const void* data) = 0;
		virtual uint64_t getNativeTextureId(TextureHandle tex) const = 0;

		// helpers
		template<typename T>
		void cmdPushConstants(CommandListHandle cmd, PipelineHandle pipeline, const T& data, ShaderStage stages = ShaderStage::Vertex | ShaderStage::Fragment) {
			static_assert(std::is_trivially_copyable_v<T>);
			cmdPushConstants(cmd, pipeline, stages, static_cast<const void*>(&data), static_cast<uint32_t>(sizeof(T)));
		}

		template<typename T>
		void updateBuffer(BufferHandle handle, const T& data, size_t offset = 0) {
			static_assert(std::is_trivially_copyable_v<T>);
			updateBuffer(handle, offset, sizeof(T), static_cast<const void*>(&data));
		}

		template<typename T>
		void cmdUpdateBuffer(CommandListHandle cmd, BufferHandle handle, const T& data, size_t offset = 0) {
			static_assert(std::is_trivially_copyable_v<T>);
			cmdUpdateBuffer(cmd, handle, offset, sizeof(T), static_cast<const void*>(&data));
		}
	};
}
