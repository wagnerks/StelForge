#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "RenderHardwareInterface.h"
#include "glad/glad.h"

namespace SFE::Render
{
	struct GLTexture { unsigned id; TextureDesc desc; };
	struct GLSampler { unsigned id;	SamplerDesc desc; };
	struct GLFramebuffer { unsigned id; FramebufferDesc desc; };
	struct GLBuffer { unsigned id = 0; GLenum target = GL_ARRAY_BUFFER; BufferDesc desc; };
	struct GLVertexArray { GLuint id = 0; VertexLayoutDesc desc; };
	struct GLShaderModule { GLuint id = 0; ShaderModuleDesc desc; };
	struct GLRenderPass
	{
		RenderPassDesc desc;
	};

	struct GLGraphicsPipeline
	{
		GLuint programId = 0;
		GLuint vaoId = 0;

		// кешируем GL-формы состояний
		GLenum primitiveGL = GL_TRIANGLES;
		GLenum cullModeGL = 0;          // 0 = no cull
		GLenum frontFaceGL = GL_CCW;
		GLenum fillModeGL = GL_FILL;
		GLenum depthFuncGL = GL_LESS;

		bool  depthTestEnabled = true;
		bool  depthWriteEnabled = true;

		bool  blendEnabled = false;
		GLenum blendSrcGL = GL_ONE;
		GLenum blendDstGL = GL_ZERO;

		GraphicsPipelineDesc desc; // опционально — для дебага / реконфига

		std::unordered_map<std::string, GLint> uniformLocations;
	};

	struct GLDescriptorBindingData
	{
		DescriptorType type;
		std::vector<DescriptorBufferInfo> buffers;
		std::vector<DescriptorImageInfo>  images;
	};

	struct GLDescriptorSet
	{
		DescriptorSetLayoutDesc layout;
		std::vector<GLDescriptorBindingData> bindings; // индекс = binding
	};


	class OpenGLRHI : public RenderHardwareInterface
	{
	public:
		void init() override;
		void cleanup() override;

		void setTextureSubImage2D(TextureHandle tex, uint32_t x, uint32_t y, uint32_t w, uint32_t h, TextureFormat fmt, const void* data) override;

		ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) override;
		void destroyShaderModule(ShaderModuleHandle handle) override;

		FramebufferHandle createFramebuffer(const FramebufferDesc& desc) override;
		void destroyFramebuffer(FramebufferHandle framebuffer) override;

		TextureHandle createTexture(const TextureDesc& desc, const void* data) override;
		void destroyTexture(TextureHandle texture) override;

		SamplerHandle createSampler(const SamplerDesc& desc) override;
		void destroySampler(SamplerHandle sampler) override;

		void blitFramebuffer(FramebufferHandle src, FramebufferHandle dst, int srcX0, int srcY0, int srcX1, int srcY1,
		                     int dstX0, int dstY0, int dstX1, int dstY1, BlitMask mask, BlitFilter filter) override;
		void bindTexture(uint32_t slot, TextureHandle tex, SamplerHandle sampler) override;
		void bindFramebuffer(FramebufferHandle framebuffer) override;

		BufferHandle createBuffer(const BufferDesc& desc, const void* initialData) override;
		void destroyBuffer(BufferHandle handle) override;
		void updateBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) override;
		void* mapBuffer(BufferHandle handle, MapMode mode) override;
		void unmapBuffer(BufferHandle handle) override;
		void bindVertexBuffer(uint32_t slot, BufferHandle handle, uint32_t offset) override;
		void bindIndexBuffer(BufferHandle handle, IndexType type, uint32_t offset) override;
		void bindUniformBuffer(uint32_t binding, BufferHandle handle) override;
		void bindStorageBuffer(uint32_t binding, BufferHandle handle) override;

		/*VertexArrayHandle createVertexArray(const VertexArrayDesc& desc) override;
		void destroyVertexArray(VertexArrayHandle handle) override;
		void bindVertexArray(VertexArrayHandle handle) override;*/

		PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
		void bindPipeline(PipelineHandle handle) override;
	private:
		std::vector<GLTexture> textures;
		std::vector<GLSampler> samplers;
		std::vector<GLFramebuffer> framebuffers;

		std::vector<GLBuffer> m_buffers;
		std::vector<GLVertexArray> m_vertexArrays;
		std::vector<GLShaderModule> m_shaders;
		std::vector<GLGraphicsPipeline> m_pipelines;

		IndexType m_currentIndexType = IndexType::Uint16;
		uint32_t m_currentIndexOffset = 0;
		GLenum m_currentPrimitive = GL_TRIANGLES;
		PipelineHandle m_currentPipeline;
		std::vector<GLRenderPass> m_renderPasses;

		std::vector<GLDescriptorSet> m_descriptorSets;
		std::vector<DescriptorSetLayoutDesc> m_descriptorSetLayouts;


		GLuint m_prevFramebuffer = 0; // для сохранения/восстановления FBO в begin/endRenderPass
		GLuint m_pushConstantUBO = 0; // constant buffer for push constants emulation
		GLuint m_pushConstantPos = 15; // binding point for push constants emulation

		GLenum toGLBufferTarget(BufferUsage usage) const;
		GLenum toGLAccessFlags(CpuAccess access) const;
		GLbitfield toGLMapFlags(MapMode mode, CpuAccess access) const;
		GLenum toGLType(VertexFormat f) const;
		GLint  toGLSize(VertexFormat f) const;

	public:
		void setUniform(PipelineHandle handle, const std::string& name, int value) override;
		void setUniform(PipelineHandle handle, const std::string& name, float value) override;
		void setUniform(PipelineHandle handle, const std::string& name, const Math::Mat4& value) override;
		void setUniform(PipelineHandle handle, const std::string& name, const Math::Vec4& value) override;
		void setUniform(PipelineHandle handle, const std::string& name, const Math::Vec3& value) override;
		void setUniform(PipelineHandle handle, const std::string& name, const Math::Vec2& value) override;
		void draw(uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount) override;
		void drawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, IndexType indexType, int32_t vertexOffset) override;
		void setUniform(PipelineHandle handle, const std::string& name, bool value) override;

		RenderPassHandle createRenderPass(const RenderPassDesc& desc) override;
		void destroyRenderPass(RenderPassHandle renderPass) override;
		void beginRenderPass(const RenderPassBeginInfo& beginInfo) override;
		void endRenderPass() override;



		CommandListHandle beginCommandList(CommandListType type) override;
		void endCommandList(CommandListHandle cmd) override;
		void submit(const CommandListHandle* lists, uint32_t count) override;
		void present() override;
		void cmdBeginRenderPass(CommandListHandle cmd, const RenderPassBeginInfo& beginInfo) override;
		void cmdEndRenderPass(CommandListHandle cmd) override;
		void cmdBindPipeline(CommandListHandle cmd, PipelineHandle pipeline) override;
		void cmdBindVertexBuffer(CommandListHandle cmd, uint32_t slot, BufferHandle handle, uint32_t offset) override;
		void cmdBindIndexBuffer(CommandListHandle cmd, BufferHandle handle, IndexType type, uint32_t offset) override;
		void cmdDraw(CommandListHandle cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
		void cmdDrawIndexed(CommandListHandle cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;
		void cmdPushConstants(CommandListHandle cmd, PipelineHandle pipeline, ShaderStage stages, const void* data,	uint32_t size) override;

		DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
		void destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout) override;
		DescriptorSetHandle allocateDescriptorSet(DescriptorSetLayoutHandle layout) override;
		void freeDescriptorSet(DescriptorSetHandle set) override;
		void updateDescriptorSets(const std::vector<WriteDescriptor>& writes) override;
		void cmdBindDescriptorSets(CommandListHandle cmd, PipelineHandle layout, uint32_t firstSet, const DescriptorSetHandle* sets, uint32_t setCount) override;
	};
}
