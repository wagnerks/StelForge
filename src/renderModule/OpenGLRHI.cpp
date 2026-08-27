#include <renderModule/openGLRHI.h>

#include <cstring>
#include <type_traits>

#include <glad/glad.h> // should be included before GLFW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <core/Engine.h>

#include <logsModule/logger.h>
#include <windowsModule/Window.h>

namespace SFE::Render
{
	static GLenum ToGLAttachmentFormat(TextureFormat f)
	{
		switch (f) {
		case TextureFormat::DEPTH_COMPONENT16:
		case TextureFormat::DEPTH_COMPONENT24:
		case TextureFormat::DEPTH_COMPONENT32:
		case TextureFormat::DEPTH_COMPONENT32F:
			return GL_DEPTH_ATTACHMENT;
		case TextureFormat::DEPTH24_STENCIL8:
		case TextureFormat::DEPTH32F_STENCIL8:
			return GL_DEPTH_STENCIL_ATTACHMENT;
		default:
			return GL_COLOR_ATTACHMENT0; // fallback
		}
	}

	static GLint ToGLInternalFormat(TextureFormat f)
	{
		switch (f) {
		case TextureFormat::R8: return GL_R8;
		case TextureFormat::R8_SNORM: return GL_R8_SNORM;
		case TextureFormat::R16: return GL_R16;
		case TextureFormat::R16_SNORM: return GL_R16_SNORM;

		case TextureFormat::RG8: return GL_RG8;
		case TextureFormat::RG8_SNORM: return GL_RG8_SNORM;
		case TextureFormat::RG16: return GL_RG16;
		case TextureFormat::RG16_SNORM: return GL_RG16_SNORM;

		case TextureFormat::R3_G3_B2: return GL_R3_G3_B2;
		case TextureFormat::RGB4: return GL_RGB4;
		case TextureFormat::RGB5: return GL_RGB5;
		case TextureFormat::RGB8: return GL_RGB8;
		case TextureFormat::RGB8_SNORM: return GL_RGB8_SNORM;
		case TextureFormat::RGB10: return GL_RGB10;
		case TextureFormat::RGB12: return GL_RGB12;
		case TextureFormat::RGB16_SNORM: return GL_RGB16_SNORM;

		case TextureFormat::RGBA2: return GL_RGBA2;
		case TextureFormat::RGBA4: return GL_RGBA4;
		case TextureFormat::RGB5_A1: return GL_RGB5_A1;
		case TextureFormat::RGBA8: return GL_RGBA8;
		case TextureFormat::RGBA8_SNORM: return GL_RGBA8_SNORM;
		case TextureFormat::RGB10_A2: return GL_RGB10_A2;
		case TextureFormat::RGB10_A2UI: return GL_RGB10_A2UI;
		case TextureFormat::RGBA12: return GL_RGBA12;
		case TextureFormat::RGBA16: return GL_RGBA16;

		case TextureFormat::SRGB8: return GL_SRGB8;
		case TextureFormat::SRGB8_ALPHA8: return GL_SRGB8_ALPHA8;

		case TextureFormat::R16F: return GL_R16F;
		case TextureFormat::RG16F: return GL_RG16F;
		case TextureFormat::RGB16F: return GL_RGB16F;
		case TextureFormat::RGBA16F: return GL_RGBA16F;

		case TextureFormat::R32F: return GL_R32F;
		case TextureFormat::RG32F: return GL_RG32F;
		case TextureFormat::RGB32F: return GL_RGB32F;
		case TextureFormat::RGBA32F: return GL_RGBA32F;

		case TextureFormat::R11F_G11F_B10F: return GL_R11F_G11F_B10F;
		case TextureFormat::RGB9_E5: return GL_RGB9_E5;

		case TextureFormat::R8I: return GL_R8I;
		case TextureFormat::R8UI: return GL_R8UI;
		case TextureFormat::R16I: return GL_R16I;
		case TextureFormat::R16UI: return GL_R16UI;
		case TextureFormat::R32I: return GL_R32I;
		case TextureFormat::R32UI: return GL_R32UI;

		case TextureFormat::RG8I: return GL_RG8I;
		case TextureFormat::RG8UI: return GL_RG8UI;
		case TextureFormat::RG16I: return GL_RG16I;
		case TextureFormat::RG16UI: return GL_RG16UI;
		case TextureFormat::RG32I: return GL_RG32I;
		case TextureFormat::RG32UI: return GL_RG32UI;

		case TextureFormat::RGB8I: return GL_RGB8I;
		case TextureFormat::RGB8UI: return GL_RGB8UI;
		case TextureFormat::RGB16I: return GL_RGB16I;
		case TextureFormat::RGB16UI: return GL_RGB16UI;
		case TextureFormat::RGB32I: return GL_RGB32I;
		case TextureFormat::RGB32UI: return GL_RGB32UI;

		case TextureFormat::RGBA8I: return GL_RGBA8I;
		case TextureFormat::RGBA8UI: return GL_RGBA8UI;
		case TextureFormat::RGBA16I: return GL_RGBA16I;
		case TextureFormat::RGBA16UI: return GL_RGBA16UI;
		case TextureFormat::RGBA32I: return GL_RGBA32I;
		case TextureFormat::RGBA32UI: return GL_RGBA32UI;

		case TextureFormat::DEPTH_COMPONENT16: return GL_DEPTH_COMPONENT16;
		case TextureFormat::DEPTH_COMPONENT24: return GL_DEPTH_COMPONENT24;
		case TextureFormat::DEPTH_COMPONENT32: return GL_DEPTH_COMPONENT32;
		case TextureFormat::DEPTH_COMPONENT32F: return GL_DEPTH_COMPONENT32F;
		case TextureFormat::DEPTH24_STENCIL8:	return GL_DEPTH24_STENCIL8;
		case TextureFormat::DEPTH32F_STENCIL8:	return GL_DEPTH32F_STENCIL8;

		default:
			return GL_RGBA8;
		}
	}

	static GLenum ToGLFormat(TextureFormat f)
	{
		switch (f) {
		case TextureFormat::R8:
		case TextureFormat::R8_SNORM:
		case TextureFormat::R16:
		case TextureFormat::R16_SNORM:
		case TextureFormat::R16F:
		case TextureFormat::R32F:
		case TextureFormat::R8I:
		case TextureFormat::R8UI:
		case TextureFormat::R16I:
		case TextureFormat::R16UI:
		case TextureFormat::R32I:
		case TextureFormat::R32UI:
			return GL_RED;

		case TextureFormat::RG8:
		case TextureFormat::RG8_SNORM:
		case TextureFormat::RG16:
		case TextureFormat::RG16_SNORM:
		case TextureFormat::RG16F:
		case TextureFormat::RG32F:
		case TextureFormat::RG8I:
		case TextureFormat::RG8UI:
		case TextureFormat::RG16I:
		case TextureFormat::RG16UI:
		case TextureFormat::RG32I:
		case TextureFormat::RG32UI:
			return GL_RG;

		case TextureFormat::R3_G3_B2:
		case TextureFormat::RGB4:
		case TextureFormat::RGB5:
		case TextureFormat::RGB8:
		case TextureFormat::RGB8_SNORM:
		case TextureFormat::RGB10:
		case TextureFormat::RGB12:
		case TextureFormat::RGB16_SNORM:
		case TextureFormat::SRGB8:
		case TextureFormat::RGB16F:
		case TextureFormat::RGB32F:
		case TextureFormat::R11F_G11F_B10F:
		case TextureFormat::RGB9_E5:
		case TextureFormat::RGB8I:
		case TextureFormat::RGB8UI:
		case TextureFormat::RGB16I:
		case TextureFormat::RGB16UI:
		case TextureFormat::RGB32I:
		case TextureFormat::RGB32UI:
			return GL_RGB;

		case TextureFormat::RGBA2:
		case TextureFormat::RGBA4:
		case TextureFormat::RGB5_A1:
		case TextureFormat::RGBA8:
		case TextureFormat::RGBA8_SNORM:
		case TextureFormat::RGB10_A2:
		case TextureFormat::SRGB8_ALPHA8:
		case TextureFormat::RGBA12:
		case TextureFormat::RGBA16:
		case TextureFormat::RGBA16F:
		case TextureFormat::RGBA32F:
		case TextureFormat::RGBA8I:
		case TextureFormat::RGBA8UI:
		case TextureFormat::RGBA16I:
		case TextureFormat::RGBA16UI:
		case TextureFormat::RGBA32I:
		case TextureFormat::RGBA32UI:
			return GL_RGBA;

		case TextureFormat::DEPTH_COMPONENT16:
		case TextureFormat::DEPTH_COMPONENT24:
		case TextureFormat::DEPTH_COMPONENT32:
		case TextureFormat::DEPTH_COMPONENT32F:
			return GL_DEPTH_COMPONENT;

		case TextureFormat::DEPTH24_STENCIL8:
		case TextureFormat::DEPTH32F_STENCIL8:
			return GL_DEPTH_STENCIL;
		default:
			return GL_RGBA;
		}
	}

	static GLenum ToGLType(TextureFormat f)
	{
		switch (f) {

			// unsigned normalized
		case TextureFormat::R8:
		case TextureFormat::RG8:
		case TextureFormat::RGB8:
		case TextureFormat::RGBA8:
			return GL_UNSIGNED_BYTE;

			// signed normalized
		case TextureFormat::R8_SNORM:
		case TextureFormat::RG8_SNORM:
		case TextureFormat::RGB8_SNORM:
		case TextureFormat::RGBA8_SNORM:
			return GL_BYTE;

		case TextureFormat::R16:
		case TextureFormat::RG16:
			return GL_UNSIGNED_SHORT;

		case TextureFormat::R16_SNORM:
		case TextureFormat::RG16_SNORM:
			return GL_SHORT;

		case TextureFormat::RGB5:
		case TextureFormat::RGB4:
		case TextureFormat::RGBA4:
		case TextureFormat::RGB5_A1:
		case TextureFormat::R3_G3_B2:
			return GL_UNSIGNED_SHORT_5_6_5;

			// float formats
		case TextureFormat::R16F:
		case TextureFormat::RG16F:
		case TextureFormat::RGB16F:
		case TextureFormat::RGBA16F:
		case TextureFormat::R11F_G11F_B10F:
		case TextureFormat::RGB9_E5:
			return GL_HALF_FLOAT;

		case TextureFormat::R32F:
		case TextureFormat::RG32F:
		case TextureFormat::RGB32F:
		case TextureFormat::RGBA32F:
			return GL_FLOAT;

			// integer formats
		case TextureFormat::R8I:
		case TextureFormat::RG8I:
		case TextureFormat::RGB8I:
		case TextureFormat::RGBA8I:
			return GL_BYTE;

		case TextureFormat::R8UI:
		case TextureFormat::RG8UI:
		case TextureFormat::RGB8UI:
		case TextureFormat::RGBA8UI:
			return GL_UNSIGNED_BYTE;

		case TextureFormat::R16I:
		case TextureFormat::RG16I:
		case TextureFormat::RGB16I:
		case TextureFormat::RGBA16I:
			return GL_SHORT;

		case TextureFormat::R16UI:
		case TextureFormat::RG16UI:
		case TextureFormat::RGB16UI:
		case TextureFormat::RGBA16UI:
			return GL_UNSIGNED_SHORT;

		case TextureFormat::R32I:
		case TextureFormat::RG32I:
		case TextureFormat::RGB32I:
		case TextureFormat::RGBA32I:
			return GL_INT;

		case TextureFormat::R32UI:
		case TextureFormat::RG32UI:
		case TextureFormat::RGB32UI:
		case TextureFormat::RGBA32UI:
			return GL_UNSIGNED_INT;

			// depth formats
		case TextureFormat::DEPTH_COMPONENT16: return GL_UNSIGNED_SHORT;
		case TextureFormat::DEPTH_COMPONENT24: return GL_UNSIGNED_INT;
		case TextureFormat::DEPTH_COMPONENT32: return GL_UNSIGNED_INT;
		case TextureFormat::DEPTH_COMPONENT32F: return GL_FLOAT;

		case TextureFormat::DEPTH24_STENCIL8:
			return GL_UNSIGNED_INT_24_8;

		case TextureFormat::DEPTH32F_STENCIL8:
			return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		default:
			return GL_UNSIGNED_BYTE;
		}
	}

	static GLenum ToOpenglTargetType(TextureDimension dimension)
	{
		switch (dimension) {
		case TextureDimension::Tex2D: return GL_TEXTURE_2D;
		case TextureDimension::Tex3D: return GL_TEXTURE_3D;
		case TextureDimension::Array: return GL_TEXTURE_2D_ARRAY;
		case TextureDimension::Cube: return GL_TEXTURE_CUBE_MAP;
		default:;
		}

		return GL_TEXTURE_2D;
	}

	static GLenum toGLShaderStage(ShaderStage stage)
	{
		switch (stage)
		{
		case ShaderStage::Vertex:      return GL_VERTEX_SHADER;
		case ShaderStage::Fragment:    return GL_FRAGMENT_SHADER;
		case ShaderStage::Geometry:    return GL_GEOMETRY_SHADER;
		case ShaderStage::Compute:     return GL_COMPUTE_SHADER;
		case ShaderStage::TessControl: return GL_TESS_CONTROL_SHADER;
		case ShaderStage::TessEval:    return GL_TESS_EVALUATION_SHADER;
		default:
			return 0;
		}
	}

	static GLenum ToGLTopology(PrimitiveTopology t)
	{
		switch (t) {
		case PrimitiveTopology::Lines:    return GL_LINES;
		case PrimitiveTopology::LineLoop:    return GL_LINE_LOOP;
		case PrimitiveTopology::Points:   return GL_POINTS;
		case PrimitiveTopology::TriangleStrip:   return GL_TRIANGLE_STRIP;
		case PrimitiveTopology::Triangles:
		default:                          return GL_TRIANGLES;
		}
	}

	static GLenum ToGLCullMode(CullMode c)
	{
		switch (c) {
		case CullMode::Front: return GL_FRONT;
		case CullMode::Back:  return GL_BACK;
		case CullMode::None:
		default:              return 0; // 0 = отключить cull
		}
	}

	static GLenum ToGLFrontFace(FrontFace f)
	{
		return (f == FrontFace::ClockWise) ? GL_CW : GL_CCW;
	}

	static GLenum ToGLFillMode(PolygonFillMode f)
	{
		return (f == PolygonFillMode::Line) ? GL_LINE : GL_FILL;
	}

	static GLenum ToGLBlendFactor(BlendFactor f)
	{
		switch (f) {
		case BlendFactor::Zero:             return GL_ZERO;
		case BlendFactor::One:              return GL_ONE;
		case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
		case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
		default:                            return GL_ONE;
		}
	}

	// Если ещё нет маппера CompareOp -> GLenum:
	static GLint ToGLCompareFunc(CompareOp op)
	{
		switch (op) {
		case CompareOp::Less:         return GL_LESS;
		case CompareOp::LessEqual:    return GL_LEQUAL;
		case CompareOp::Greater:      return GL_GREATER;
		case CompareOp::GreaterEqual: return GL_GEQUAL;
		case CompareOp::Equal:        return GL_EQUAL;
		case CompareOp::NotEqual:     return GL_NOTEQUAL;
		case CompareOp::Always:       return GL_ALWAYS;
		case CompareOp::Never:        return GL_NEVER;
		case CompareOp::None:
		default:                      return GL_ALWAYS;
		}
	}

	static GLenum ToGLIndexType(IndexType t)
	{
		switch (t) {
		case IndexType::Uint16: return GL_UNSIGNED_SHORT;
		case IndexType::Uint32:
		default:                return GL_UNSIGNED_INT;
		}
	}

	GLenum OpenGLRHI::toGLBufferTarget(BufferUsage usage) const
	{
		switch (usage)
		{
		case BufferUsage::Vertex:   return GL_ARRAY_BUFFER;
		case BufferUsage::Index:    return GL_ELEMENT_ARRAY_BUFFER;
		case BufferUsage::Uniform:  return GL_UNIFORM_BUFFER;
		case BufferUsage::Storage:  return GL_SHADER_STORAGE_BUFFER;
		case BufferUsage::Indirect: return GL_DRAW_INDIRECT_BUFFER;
		default:                    return GL_ARRAY_BUFFER;
		}
	}

	GLenum OpenGLRHI::toGLAccessFlags(CpuAccess access) const
	{
		// Используется как hint для glBufferData (usage)
		switch (access)
		{
		case CpuAccess::None:      return GL_STATIC_DRAW;
		case CpuAccess::Write:     return GL_DYNAMIC_DRAW;
		case CpuAccess::Read:      return GL_STATIC_READ;
		case CpuAccess::ReadWrite: return GL_DYNAMIC_READ;
		default:                   return GL_DYNAMIC_DRAW;
		}
	}

	GLbitfield OpenGLRHI::toGLMapFlags(MapMode mode, CpuAccess access) const
	{
		GLbitfield flags = 0;

		// Доступ по направлению
		switch (mode)
		{
		case MapMode::Read:
			flags |= GL_MAP_READ_BIT;
			break;
		case MapMode::Write:
			flags |= GL_MAP_WRITE_BIT;
			break;
		case MapMode::ReadWrite:
			flags |= (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
			break;
		}

		// Если это CPU→GPU обновление, можно добавить invalidate/unsynchronized
		if (access == CpuAccess::Write || access == CpuAccess::ReadWrite)
		{
			flags |= GL_MAP_INVALIDATE_RANGE_BIT;
			flags |= GL_MAP_UNSYNCHRONIZED_BIT;
		}

		return flags;
	}

	bool isInteger(VertexFormat f)
	{
		switch (f) {
		case VertexFormat::Float:
		case VertexFormat::Float2:
		case VertexFormat::Float3:
		case VertexFormat::Float4:
			return false;
		case VertexFormat::Int:
		case VertexFormat::Int2:
		case VertexFormat::Int3:
		case VertexFormat::Int4:
		case VertexFormat::UInt:
		case VertexFormat::UInt2:
		case VertexFormat::UInt3:
		case VertexFormat::UInt4:
			return true;
		default:
			return false;
		}
	}


	GLenum OpenGLRHI::toGLType(VertexFormat f) const
	{
		switch (f) {
		case VertexFormat::Float:
		case VertexFormat::Float2:
		case VertexFormat::Float3:
		case VertexFormat::Float4:
			return GL_FLOAT;
		case VertexFormat::Int:
		case VertexFormat::Int2:
		case VertexFormat::Int3:
		case VertexFormat::Int4:
			return GL_INT;
		case VertexFormat::UInt:
		case VertexFormat::UInt2:
		case VertexFormat::UInt3:
		case VertexFormat::UInt4:
			return GL_UNSIGNED_INT;
		default:
			return GL_FLOAT;
		}
	}

	GLint OpenGLRHI::toGLSize(VertexFormat f) const
	{
		switch (f)
		{
		case VertexFormat::Float:
		case VertexFormat::Int:
		case VertexFormat::UInt:

			return 1;
		case VertexFormat::Float2:
		case VertexFormat::Int2:
		case VertexFormat::UInt2:
			return 2;
		case VertexFormat::Float3:
		case VertexFormat::Int3:
		case VertexFormat::UInt3:
			return 3;
		case VertexFormat::Float4:
		case VertexFormat::Int4:
		case VertexFormat::UInt4:
			return 4;
		default:
			return 3;
		}
	}


	GLCommandList* OpenGLRHI::recordingList(CommandListHandle cmd)
	{
		GLCommandList* list = getLive(m_commandLists, cmd.id);
		if (!list || !list->recording) {
			return nullptr;
		}
		return list;
	}

	void OpenGLRHI::cmdEndRenderPass(CommandListHandle cmd)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdEndRenderPass{});
		}
	}

	void OpenGLRHI::execEndRenderPass()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_prevFramebuffer);
		m_prevFramebuffer = 0;
	}

	void OpenGLRHI::cmdBindPipeline(CommandListHandle cmd, PipelineHandle pipeline)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdBindPipeline{ pipeline });
		}
	}

	void OpenGLRHI::execBindPipeline(PipelineHandle pipeline)
	{
		const GLGraphicsPipeline* gpPtr = getLive(m_pipelines, pipeline.id);
		if (!gpPtr) {
			return;
		}

		const GLGraphicsPipeline& gp = *gpPtr;

		// шейдерная программа
		glUseProgram(gp.programId);
		glBindVertexArray(gp.vaoId);

		// topology
		m_currentPrimitive = gp.primitiveGL;

		// polygon mode
		glPolygonMode(GL_FRONT_AND_BACK, gp.fillModeGL);

		// cull
		if (gp.cullModeGL == 0) {
			glDisable(GL_CULL_FACE);
		}
		else {
			glEnable(GL_CULL_FACE);
			glCullFace(gp.cullModeGL);
		}
		glFrontFace(gp.frontFaceGL);

		// depth
		if (gp.depthTestEnabled) {
			glEnable(GL_DEPTH_TEST);
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}
		glDepthFunc(gp.depthFuncGL);
		glDepthMask(gp.depthWriteEnabled ? GL_TRUE : GL_FALSE);

		// blend
		if (gp.blendEnabled) {
			glEnable(GL_BLEND);
			glBlendFunc(gp.blendSrcGL, gp.blendDstGL);
		}
		else {
			glDisable(GL_BLEND);
		}

		m_currentPipeline = pipeline;
	}

	void OpenGLRHI::cmdBindVertexBuffer(CommandListHandle cmd, uint32_t slot, BufferHandle handle, uint32_t offset)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdBindVertexBuffer{ slot, handle, offset });
		}
	}

	void OpenGLRHI::execBindVertexBuffer(uint32_t slot, BufferHandle handle, uint32_t offset)
	{
		const auto* gp = getLive(m_pipelines, m_currentPipeline.id);
		const auto* buf = getLive(m_buffers, handle.id);
		if (!gp || !buf)
			return;

		// защитимся от кривого slot
		if (slot >= gp->desc.layout.bindings.size())
			return;

		SFE::LogsModule::Logger::LOG_FATAL(buf->desc.usage == BufferUsage::Vertex, "WRONG BUFFER TYPE BINDING");
		const auto& bindingDesc = gp->desc.layout.bindings[slot];
		const GLsizei stride = static_cast<GLsizei>(bindingDesc.stride);
		glVertexArrayVertexBuffer(
			gp->vaoId,
			bindingDesc.binding,                          // vertex binding
			buf->id,                        // VBO
			static_cast<GLintptr>(offset), // byte offset
			stride
		);
		glBindBuffer(buf->target, buf->id);
	}

	void OpenGLRHI::cmdBindIndexBuffer(CommandListHandle cmd, BufferHandle handle, IndexType type, uint32_t offset)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdBindIndexBuffer{ handle, type, offset });
		}
	}

	void OpenGLRHI::execBindIndexBuffer(BufferHandle handle, IndexType type, uint32_t offset)
	{
		const auto* gp = getLive(m_pipelines, m_currentPipeline.id);
		const auto* buf = getLive(m_buffers, handle.id);
		if (!gp || !buf) {
			m_currentIndexType = type;
			m_currentIndexOffset = 0;
			return;
		}

		SFE::LogsModule::Logger::LOG_FATAL(buf->desc.usage == BufferUsage::Index, "WRONG BUFFER TYPE BINDING");
		glVertexArrayElementBuffer(gp->vaoId, buf->id);
		glBindBuffer(buf->target, buf->id);
		m_currentIndexType = type;
		m_currentIndexOffset = offset;
	}

	void OpenGLRHI::cmdDraw(CommandListHandle cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdDraw{ vertexCount, instanceCount, firstVertex, firstInstance });
		}
	}

	void OpenGLRHI::execDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		if (vertexCount == 0 || instanceCount == 0) {
			return;
		}

		if (instanceCount == 1 && firstInstance == 0) {
			glDrawArrays(m_currentPrimitive,
				static_cast<GLint>(firstVertex),
				static_cast<GLsizei>(vertexCount));
		}
		else {
			glDrawArraysInstanced(m_currentPrimitive,
				static_cast<GLint>(firstVertex),
				static_cast<GLsizei>(vertexCount),
				static_cast<GLsizei>(instanceCount));
		}
	}

	void OpenGLRHI::cmdDrawIndexed(CommandListHandle cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdDrawIndexed{ indexCount, instanceCount, firstIndex, vertexOffset, firstInstance });
		}
	}

	void OpenGLRHI::execDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		if (indexCount == 0 || instanceCount == 0) {
			return;
		}

		const GLenum glIndexType = ToGLIndexType(m_currentIndexType);
		const GLsizei indexSize = (m_currentIndexType == IndexType::Uint16) ? sizeof(uint16_t) : sizeof(uint32_t);

		uintptr_t baseOffsetBytes = static_cast<uintptr_t>(m_currentIndexOffset);
		uintptr_t firstIndexBytes = static_cast<uintptr_t>(firstIndex) * indexSize;
		const void* indexOffset = reinterpret_cast<const void*>(baseOffsetBytes + firstIndexBytes);

		if (vertexOffset == 0) {
			glDrawElementsInstanced(
				m_currentPrimitive,
				static_cast<GLsizei>(indexCount),
				glIndexType,
				indexOffset,
				static_cast<GLsizei>(instanceCount)
			);
		}
		else {
			glDrawElementsInstancedBaseVertex(
				m_currentPrimitive,
				static_cast<GLsizei>(indexCount),
				glIndexType,
				indexOffset,
				static_cast<GLsizei>(instanceCount),
				vertexOffset
			);
		}
	}

	RenderPassHandle OpenGLRHI::createRenderPass(const RenderPassDesc& desc)
	{
		// В OpenGL рендер-пасс — чисто описание: LoadOp/StoreOp/clear-значения.
		// Никаких GL-объектов создавать не нужно.
		GLRenderPass rp{};
		rp.desc = desc;
		rp.live = true;

		RenderPassHandle handle;
		handle.id = storeResource(m_renderPasses, m_freeRenderPasses, std::move(rp));
		return handle;
	}

	void OpenGLRHI::destroyRenderPass(RenderPassHandle renderPass)
	{
		recycleResource(m_renderPasses, m_freeRenderPasses, renderPass.id);
	}

	CommandListHandle OpenGLRHI::beginCommandList(CommandListType type)
	{
		GLCommandList list{};
		list.type = type;
		list.recording = true;
		list.live = true;

		CommandListHandle handle;
		handle.id = storeResource(m_commandLists, m_freeCommandLists, std::move(list));
		return handle;
	}

	void OpenGLRHI::endCommandList(CommandListHandle cmd)
	{
		if (GLCommandList* list = getLive(m_commandLists, cmd.id)) {
			list->recording = false;
		}
	}

	void OpenGLRHI::executeCommand(const GLCommand& command)
	{
		std::visit([this](const auto& recorded) {
			using T = std::decay_t<decltype(recorded)>;
			if constexpr (std::is_same_v<T, CmdBeginRenderPass>) {
				execBeginRenderPass(recorded);
			}
			else if constexpr (std::is_same_v<T, CmdEndRenderPass>) {
				execEndRenderPass();
			}
			else if constexpr (std::is_same_v<T, CmdBindPipeline>) {
				execBindPipeline(recorded.pipeline);
			}
			else if constexpr (std::is_same_v<T, CmdBindVertexBuffer>) {
				execBindVertexBuffer(recorded.slot, recorded.handle, recorded.offset);
			}
			else if constexpr (std::is_same_v<T, CmdBindIndexBuffer>) {
				execBindIndexBuffer(recorded.handle, recorded.type, recorded.offset);
			}
			else if constexpr (std::is_same_v<T, CmdBindDescriptorSets>) {
				execBindDescriptorSets(recorded.bindings);
			}
			else if constexpr (std::is_same_v<T, CmdPushConstants>) {
				execPushConstants(recorded.data.data(), static_cast<uint32_t>(recorded.data.size()));
			}
			else if constexpr (std::is_same_v<T, CmdDraw>) {
				execDraw(recorded.vertexCount, recorded.instanceCount, recorded.firstVertex, recorded.firstInstance);
			}
			else if constexpr (std::is_same_v<T, CmdDrawIndexed>) {
				execDrawIndexed(recorded.indexCount, recorded.instanceCount, recorded.firstIndex, recorded.vertexOffset, recorded.firstInstance);
			}
			else if constexpr (std::is_same_v<T, CmdBlit>) {
				execBlit(recorded.src, recorded.dst, recorded.mask, recorded.filter);
			}
			else if constexpr (std::is_same_v<T, CmdUpdateBuffer>) {
				execUpdateBuffer(recorded.handle, recorded.offset, recorded.data.size(), recorded.data.data());
			}
			else if constexpr (std::is_same_v<T, CmdPipelineBarrier>) {
				execPipelineBarrier(recorded.barrier);
			}
		}, command);
	}

	void OpenGLRHI::submit(const CommandListHandle* lists, uint32_t count)
	{
		if (!lists) {
			return;
		}

		for (uint32_t i = 0; i < count; ++i) {
			GLCommandList* list = getLive(m_commandLists, lists[i].id);
			if (!list) {
				continue;
			}
			list->recording = false;
			for (const auto& command : list->commands) {
				executeCommand(command);
			}
			recycleResource(m_commandLists, m_freeCommandLists, lists[i].id);
		}
	}

	void OpenGLRHI::present()
	{
		if (auto window = Engine::instance()->getWindow()) {
			window->swapBuffers();
		}
	}

	void OpenGLRHI::cmdBeginRenderPass(CommandListHandle cmd, const RenderPassBeginInfo& beginInfo)
	{
		GLCommandList* list = recordingList(cmd);
		if (!list) {
			return;
		}

		CmdBeginRenderPass rec;
		rec.info = beginInfo;
		rec.info.overrideClearColors = nullptr;
		rec.info.overrideClearDepth = nullptr;
		rec.info.overrideClearStencil = nullptr;
		if (beginInfo.overrideClearColors) {
			rec.hasOverrideColors = true;
			const GLRenderPass* rp = getLive(m_renderPasses, beginInfo.renderPass.id);
			const uint32_t n = rp ? rp->desc.colorAttachmentCount : 8u;
			for (uint32_t i = 0; i < n && i < 8u; ++i) {
				rec.clearColors[i] = beginInfo.overrideClearColors[i];
			}
		}
		if (beginInfo.overrideClearDepth) {
			rec.hasOverrideDepth = true;
			rec.clearDepth = *beginInfo.overrideClearDepth;
		}
		if (beginInfo.overrideClearStencil) {
			rec.hasOverrideStencil = true;
			rec.clearStencil = *beginInfo.overrideClearStencil;
		}
		list->commands.emplace_back(std::move(rec));
	}

	void OpenGLRHI::execBeginRenderPass(const CmdBeginRenderPass& recorded)
	{
		RenderPassBeginInfo beginInfo = recorded.info;
		if (recorded.hasOverrideColors) {
			beginInfo.overrideClearColors = recorded.clearColors;
		}
		if (recorded.hasOverrideDepth) {
			beginInfo.overrideClearDepth = &recorded.clearDepth;
		}
		if (recorded.hasOverrideStencil) {
			beginInfo.overrideClearStencil = &recorded.clearStencil;
		}

		const GLRenderPass* rpPtr = getLive(m_renderPasses, beginInfo.renderPass.id);
		if (!rpPtr) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::beginRenderPass: invalid RenderPass handle");
			return;
		}

		const GLRenderPass& rp = *rpPtr;


		// Сохраняем предыдущий FBO, чтобы вернуть в endRenderPass
		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
		m_prevFramebuffer = static_cast<GLuint>(prevFbo);
		int w = 0;
		int h = 0;
		if (!beginInfo.framebuffer) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			auto& screen = Engine::instance()->getWindow()->getScreenData();
			w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(screen.width);
			h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(screen.height);
		}
		else if (const GLFramebuffer* fb = getLive(framebuffers, beginInfo.framebuffer.id)) {
			glBindFramebuffer(GL_FRAMEBUFFER, fb->id);
			w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(fb->desc.width);
			h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(fb->desc.height);
		}
		else {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::beginRenderPass: invalid Framebuffer handle");
			return;
		}


		// Вычисляем область
		int x = beginInfo.x;
		int y = beginInfo.y;
		//int w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(fb.desc.width);
		//int h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(fb.desc.height);

		glViewport(x, y, w, h);
		glDepthRange(0.f, 1.f);

		// --- Clear color attachments (если loadOp == Clear) ---
		for (uint32_t i = 0; i < rp.desc.colorAttachmentCount; ++i)
		{
			const auto& att = rp.desc.colorAttachments[i];
			if (att.loadOp != AttachmentLoadOp::Clear)
				continue;

			Math::Vec4 color = att.clearColor;
			if (beginInfo.overrideClearColors) {
				// предполагаем, что вызывающий гарантирует нужный размер массива
				color = beginInfo.overrideClearColors[i];
			}

			float c[4] = { color.x, color.y, color.z, color.w };
			glClearBufferfv(GL_COLOR, static_cast<GLint>(i), c);
		}

		// --- Clear depth/stencil (если есть и loadOp == Clear) ---
		if (rp.desc.hasDepthStencil && rp.desc.depthAttachment.loadOp == AttachmentLoadOp::Clear)
		{
			float depth = rp.desc.depthAttachment.clearDepth;
			uint8_t stencil = rp.desc.depthAttachment.clearStencil;

			if (beginInfo.overrideClearDepth) {
				depth = *beginInfo.overrideClearDepth;
			}
			if (rp.desc.depthAttachment.hasStencil && beginInfo.overrideClearStencil) {
				stencil = *beginInfo.overrideClearStencil;
			}

			// На время clear depth буфера логично обеспечить включённую запись по глубине
			GLboolean oldDepthMask;
			glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);
			glDepthMask(GL_TRUE);

			if (rp.desc.depthAttachment.hasStencil) {
				glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, static_cast<GLint>(stencil));
			}
			else {
				glClearBufferfv(GL_DEPTH, 0, &depth);
			}

			glDepthMask(oldDepthMask);
		}
	}

	void OpenGLRHI::cmdPushConstants(CommandListHandle cmd, PipelineHandle, ShaderStage, const void* data, uint32_t size)
	{
		GLCommandList* list = recordingList(cmd);
		if (!list || !data || size == 0) {
			return;
		}

		CmdPushConstants rec;
		rec.data.resize(size);
		std::memcpy(rec.data.data(), data, size);
		list->commands.emplace_back(std::move(rec));
	}

	void OpenGLRHI::execPushConstants(const void* data, uint32_t size)
	{
		if (!data || size == 0) {
			return;
		}

		// emulation push constant in GL via UBO
		if (!m_pushConstantUBO) {
			glCreateBuffers(1, &m_pushConstantUBO);
			glNamedBufferData(m_pushConstantUBO, 256, nullptr, GL_DYNAMIC_DRAW);
		}

		glNamedBufferSubData(m_pushConstantUBO, 0, size, data);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_pushConstantPos, m_pushConstantUBO);
	}

	DescriptorSetLayoutHandle OpenGLRHI::createDescriptorSetLayout(
		const DescriptorSetLayoutDesc& desc)
	{
		GLDescriptorSetLayout layout{};
		layout.desc = desc;
		layout.live = true;

		DescriptorSetLayoutHandle h;
		h.id = storeResource(m_descriptorSetLayouts, m_freeDescriptorSetLayouts, std::move(layout));
		return h;
	}


	void OpenGLRHI::destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout)
	{
		recycleResource(m_descriptorSetLayouts, m_freeDescriptorSetLayouts, layout.id);
	}

	DescriptorSetHandle OpenGLRHI::allocateDescriptorSet(DescriptorSetLayoutHandle layout)
	{
		const GLDescriptorSetLayout* layoutObj = getLive(m_descriptorSetLayouts, layout.id);
		if (!layoutObj)
			return {};

		GLDescriptorSet set{};
		set.layout = layoutObj->desc;
		set.live = true;
		set.bindings.reserve(set.layout.bindings.size());
		for (const auto& b : set.layout.bindings) {
			GLDescriptorBindingData data;
			data.binding = b.binding;
			data.type = b.type;
			set.bindings.push_back(std::move(data));
		}

		DescriptorSetHandle h;
		h.id = storeResource(m_descriptorSets, m_freeDescriptorSets, std::move(set));
		return h;
	}


	void OpenGLRHI::freeDescriptorSet(DescriptorSetHandle set)
	{
		recycleResource(m_descriptorSets, m_freeDescriptorSets, set.id);
	}

	void OpenGLRHI::updateDescriptorSets(const std::vector<WriteDescriptor>& writes)
	{
		for (const auto& w : writes)
		{
			GLDescriptorSet* set = getLive(m_descriptorSets, w.set.id);
			if (!set)
				continue;

			GLDescriptorBindingData* slot = nullptr;
			for (auto& b : set->bindings) {
				if (b.binding == w.binding && b.type == w.type) {
					slot = &b;
					break;
				}
			}
			if (!slot) {
				set->bindings.push_back({ w.binding, w.type, {}, {} });
				slot = &set->bindings.back();
			}

			switch (w.type)
			{
			case DescriptorType::UniformBuffer:
			case DescriptorType::StorageBuffer:
				slot->buffers = w.buffers;
				break;

			case DescriptorType::CombinedImageSampler:
				slot->images = w.images;
				break;
			}
		}
	}


	void OpenGLRHI::cmdBindDescriptorSets(CommandListHandle cmd, PipelineHandle, uint32_t, const DescriptorSetHandle* sets, uint32_t setCount)
	{
		GLCommandList* list = recordingList(cmd);
		if (!list || !sets || setCount == 0) {
			return;
		}

		CmdBindDescriptorSets rec;
		for (uint32_t i = 0; i < setCount; ++i) {
			const GLDescriptorSet* set = getLive(m_descriptorSets, sets[i].id);
			if (!set) {
				continue;
			}
			rec.bindings.insert(rec.bindings.end(), set->bindings.begin(), set->bindings.end());
		}
		list->commands.emplace_back(std::move(rec));
	}

	void OpenGLRHI::execBindDescriptorSets(const std::vector<GLDescriptorBindingData>& bindings)
	{
		// Для OpenGL считаем:
		// texture unit = binding (пока без извращений)
		// UBO/SSBO = glBindBufferBase(binding)

		for (const auto& b : bindings)
		{
			switch (b.type)
			{
			case DescriptorType::UniformBuffer:
				for (const auto& bufInfo : b.buffers)
				{
					const auto* glb = getLive(m_buffers, bufInfo.buffer.id);
					if (!glb)
						continue;
					glBindBufferBase(GL_UNIFORM_BUFFER, b.binding, glb->id);
				}
				break;

			case DescriptorType::StorageBuffer:
				for (const auto& bufInfo : b.buffers)
				{
					const auto* glb = getLive(m_buffers, bufInfo.buffer.id);
					if (!glb)
						continue;
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, b.binding, glb->id);
				}
				break;

			case DescriptorType::CombinedImageSampler:
				if (!b.images.empty())
				{
					const auto& img = b.images[0];
					if (const auto* t = getLive(textures, img.texture.id))
					{
						glActiveTexture(GL_TEXTURE0 + b.binding);
						glBindTexture(ToOpenglTargetType(t->desc.dimension), t->id);

						if (const auto* samp = getLive(samplers, img.sampler.id))
							glBindSampler(b.binding, samp->id);
					}
				}
				break;
			}
		}
	}

	FramebufferHandle OpenGLRHI::getBackbufferFramebuffer()
	{
		return {};
	}


	PipelineHandle OpenGLRHI::createGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		if (desc.shaders.vertex.id == UINT32_MAX ||
			desc.shaders.fragment.id == UINT32_MAX)
		{
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::createGraphicsPipeline: vertex/fragment shader is required");
			return {};
		}

		GLuint program = glCreateProgram();
		if (program == 0) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::createGraphicsPipeline: glCreateProgram failed");
			return {};
		}

		auto attachIfValid = [&](ShaderModuleHandle h)
		{
			const GLShaderModule* mod = getLive(m_shaders, h.id);
			if (!mod || mod->id == 0)
				return;
			glAttachShader(program, mod->id);
		};

		attachIfValid(desc.shaders.vertex);
		attachIfValid(desc.shaders.fragment);
		attachIfValid(desc.shaders.geometry);
		attachIfValid(desc.shaders.tessControl);
		attachIfValid(desc.shaders.tessEval);

		glLinkProgram(program);

		// проверка линковки
		GLint linkStatus = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus) {
			GLint logLen = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

			std::string log;
			log.resize(static_cast<size_t>(logLen));
			glGetProgramInfoLog(program, logLen, nullptr, log.data());

			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::createGraphicsPipeline: link failed:\n%s", log.c_str());

			glDeleteProgram(program);
			return {};
		}

		auto bindUniformBlock = [program](const char* name, GLuint binding) {
			const GLuint index = glGetUniformBlockIndex(program, name);
			if (index != GL_INVALID_INDEX) {
				glUniformBlockBinding(program, index, binding);
			}
		};
		bindUniformBlock("LightSpaceMatrices", 11);
		bindUniformBlock("CurrentCascade", 4);
		bindUniformBlock("PointLightMatrices", 3);
		bindUniformBlock("cascadedShadowBlock", 9);
		bindUniformBlock("pointLightsBlock", 10);
		bindUniformBlock("SharedMatrices", 5);
		bindUniformBlock("PushConstants", 15);

		{
			glUseProgram(program);
			auto setSampler = [program](const char* name, GLint unit) {
				const GLint loc = glGetUniformLocation(program, name);
				if (loc >= 0) {
					glProgramUniform1i(program, loc, unit);
				}
			};
			setSampler("gPosition", 0);
			setSampler("gNormal", 1);
			setSampler("gAlbedoSpec", 2);
			setSampler("ssao", 3);
			setSampler("shadows", 4);
			setSampler("gOutlines", 5);
			setSampler("PointLightShadowMapArray", 7);
			setSampler("shadowMap", 8);
			glUseProgram(0);
		}

		// можно сразу отвязать шейдеры
		auto detachIfValid = [&](ShaderModuleHandle h)
		{
			const GLShaderModule* mod = getLive(m_shaders, h.id);
			if (!mod || mod->id == 0)
				return;
			glDetachShader(program, mod->id);
		};

		detachIfValid(desc.shaders.vertex);
		detachIfValid(desc.shaders.fragment);
		detachIfValid(desc.shaders.geometry);
		detachIfValid(desc.shaders.tessControl);
		detachIfValid(desc.shaders.tessEval);

		// Заполняем GLGraphicsPipeline
		GLGraphicsPipeline gp{};
		gp.programId = program;
		gp.desc = desc;

		gp.primitiveGL = ToGLTopology(desc.topology);
		gp.cullModeGL = ToGLCullMode(desc.cullMode);
		gp.frontFaceGL = ToGLFrontFace(desc.frontFace);
		gp.fillModeGL = ToGLFillMode(desc.fillMode);

		gp.depthTestEnabled = desc.depthStencilState.depthTest;
		gp.depthWriteEnabled = desc.depthStencilState.depthWrite;
		gp.depthFuncGL = ToGLCompareFunc(desc.depthStencilState.depthCompareOp);

		gp.blendEnabled = desc.blend.enabled;
		gp.blendSrcGL = ToGLBlendFactor(desc.blend.srcColor);
		gp.blendDstGL = ToGLBlendFactor(desc.blend.dstColor);


		// ------------ НОВОЕ: создаём VAO и настраиваем layout ------------
		glCreateVertexArrays(1, &gp.vaoId);

		const auto& layout = desc.layout;

		// 1) binding'и: stride + divisor
		for (const VertexBindingDesc& b : layout.bindings)
		{
			// stride пока можем взять отсюда, буфер подцепим позже через bindVertexBuffer
			// divisor: instancing
			GLuint divisor = (b.inputRate == VertexInputRate::PerInstance) ? 1u : 0u;
			glVertexArrayBindingDivisor(gp.vaoId, b.binding, divisor);
		}

		// 2) атрибуты: location, binding, формат, offset
		for (const VertexAttributeDesc& a : layout.attributes)
		{
			glEnableVertexArrayAttrib(gp.vaoId, a.location);

			// Привязка атрибута к binding-слоту
			glVertexArrayAttribBinding(gp.vaoId, a.location, a.binding);

			// Формат: float / int / uint – зависит от VertexFormat
			GLenum type = toGLType(a.format);
			GLint  size = toGLSize(a.format);
			GLboolean normalized = a.normalized ? GL_TRUE : GL_FALSE;
			
			if (isInteger(a.format))
			{
				glVertexArrayAttribIFormat(
					gp.vaoId,
					a.location,
					size,
					type,
					a.offset
				);
			}
			else
			{
				glVertexArrayAttribFormat(
					gp.vaoId,
					a.location,
					size,
					type,
					normalized,
					a.offset
				);
			}
		}

		// -----------------------------------------------------------------

		PipelineHandle handle;
		gp.live = true;
		handle.id = storeResource(m_pipelines, m_freePipelines, std::move(gp));

		return handle;
	}

	void OpenGLRHI::destroyGraphicsPipeline(PipelineHandle pipeline)
	{
		GLGraphicsPipeline* gp = getLive(m_pipelines, pipeline.id);
		if (!gp) {
			return;
		}

		if (m_currentPipeline.id == pipeline.id) {
			m_currentPipeline = {};
			glUseProgram(0);
			glBindVertexArray(0);
		}

		if (gp->programId != 0) {
			glDeleteProgram(gp->programId);
		}
		if (gp->vaoId != 0) {
			glDeleteVertexArrays(1, &gp->vaoId);
		}
		recycleResource(m_pipelines, m_freePipelines, pipeline.id);
	}

	void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id,
		GLenum severity, GLsizei length,
		const GLchar* message, const void* userParam)
	{
		// тут свой логгер
		SFE::LogsModule::Logger::LOG_ERROR("GL DEBUG: %s", message); 
	}

	void InitGLDebug()
	{
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(glDebugCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
			0, nullptr, GL_TRUE);
	}

	void OpenGLRHI::init() {
		if (m_inited) {
			return;
		}

		auto window = Engine::instance()->getWindow();
		if (!window) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Try to initialize render without context, forgot to call createWindow?");
			return;
		}
		//InitGLDebug();

		glfwMakeContextCurrent(window->getWindow());
		LogsModule::Logger::LOG_FATAL(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "Failed to initialize GLAD");
		m_inited = true;


		glViewport(0, 0, window->getScreenData().renderW, window->getScreenData().renderH);
		glClearColor(0,0,0,0);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glClearDepth(1.f);
	}

	void OpenGLRHI::cleanup()
	{
		if (m_inited) {
			for (auto& gp : m_pipelines) {
				if (!gp.live) {
					continue;
				}
				if (gp.programId != 0) {
					glDeleteProgram(gp.programId);
				}
				if (gp.vaoId != 0) {
					glDeleteVertexArrays(1, &gp.vaoId);
				}
			}
			for (auto& fb : framebuffers) {
				if (fb.live && fb.id != 0) {
					glDeleteFramebuffers(1, &fb.id);
				}
			}
			for (auto& tex : textures) {
				if (tex.live && tex.id != 0) {
					glDeleteTextures(1, &tex.id);
				}
			}
			for (auto& samp : samplers) {
				if (samp.live && samp.id != 0) {
					glDeleteSamplers(1, &samp.id);
				}
			}
			for (auto& buf : m_buffers) {
				if (buf.live && buf.id != 0) {
					glDeleteBuffers(1, &buf.id);
				}
			}
			for (auto& sh : m_shaders) {
				if (sh.live && sh.id != 0) {
					glDeleteShader(sh.id);
				}
			}
			if (m_pushConstantUBO != 0) {
				glDeleteBuffers(1, &m_pushConstantUBO);
				m_pushConstantUBO = 0;
			}
		}

		textures.clear();
		samplers.clear();
		framebuffers.clear();
		m_buffers.clear();
		m_vertexArrays.clear();
		m_shaders.clear();
		m_pipelines.clear();
		m_renderPasses.clear();
		m_descriptorSets.clear();
		m_descriptorSetLayouts.clear();
		m_commandLists.clear();

		m_freeTextures.clear();
		m_freeSamplers.clear();
		m_freeFramebuffers.clear();
		m_freeBuffers.clear();
		m_freeShaders.clear();
		m_freePipelines.clear();
		m_freeRenderPasses.clear();
		m_freeDescriptorSets.clear();
		m_freeDescriptorSetLayouts.clear();
		m_freeCommandLists.clear();

		m_currentPipeline = {};
		m_prevFramebuffer = 0;
		m_inited = false;
	}

	FramebufferHandle OpenGLRHI::createFramebuffer(const FramebufferDesc& desc)
	{
		GLuint fbo = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// COLOR attachments
		std::vector<GLenum> drawBuffers;
		drawBuffers.reserve(desc.colorAttachments.size());

		SFE::LogsModule::Logger::LOG_FATAL(desc.colorAttachments.size() <= GL_MAX_COLOR_ATTACHMENTS,"Too many color attachments");

		for (uint32_t i = 0; i < desc.colorAttachments.size(); ++i) {
			const auto& att = desc.colorAttachments[i];
			const GLTexture* tex = getLive(textures, att.texture.id);
			if (!tex) {
				SFE::LogsModule::Logger::LOG_FATAL(false, "Failed to create framebuffer object!");
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDeleteFramebuffers(1, &fbo);
				return {};
			}

			GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
			drawBuffers.push_back(attachment);

			switch (tex->desc.dimension) {
				case TextureDimension::Tex2D:
					glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex->id, static_cast<GLint>(att.mipLevel));
					break;
				case TextureDimension::Tex3D:
					SFE::LogsModule::Logger::LOG_FATAL(false, "Failed to create framebuffer object!");
					break;
				default:;
			}
		}

		if (!drawBuffers.empty()) {
			glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
		}
		else {
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}

		if (desc.depthStencilAttachment.has_value()) {
			const auto& att = *desc.depthStencilAttachment;
			const GLTexture* tex = getLive(textures, att.texture.id);
			if (!tex) {
				SFE::LogsModule::Logger::LOG_FATAL(false, "Failed to create framebuffer object!");
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDeleteFramebuffers(1, &fbo);
				return {};
			}

			switch (tex->desc.dimension) {
			case TextureDimension::Tex2D:
				glFramebufferTexture2D(GL_FRAMEBUFFER, ToGLAttachmentFormat(tex->desc.format), GL_TEXTURE_2D, tex->id, static_cast<GLint>(att.mipLevel));
				break;
			case TextureDimension::Array:
			case TextureDimension::Tex3D:
				if (att.layer == UINT32_MAX) {
					glFramebufferTexture(GL_FRAMEBUFFER,
						ToGLAttachmentFormat(tex->desc.format),
						tex->id,
						static_cast<GLint>(att.mipLevel));
				}
				else {
					glFramebufferTextureLayer(GL_FRAMEBUFFER,
						ToGLAttachmentFormat(tex->desc.format),
						tex->id,
						static_cast<GLint>(att.mipLevel),
						static_cast<GLint>(att.layer));
				}
				break;
			default:;
			}
		}

		if (const GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER); fbStatus != GL_FRAMEBUFFER_COMPLETE) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Failed to create framebuffer object! status=0x%x", fbStatus);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &fbo);
			return {};
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		GLFramebuffer glfb{};
		glfb.id = fbo;
		glfb.desc = desc;
		glfb.live = true;

		FramebufferHandle handle;
		handle.id = storeResource(framebuffers, m_freeFramebuffers, std::move(glfb));
		return handle;
	}

	void OpenGLRHI::destroyFramebuffer(FramebufferHandle framebuffer)
	{
		GLFramebuffer* fb = getLive(framebuffers, framebuffer.id);
		if (!fb) {
			return;
		}
		if (m_prevFramebuffer == fb->id) {
			m_prevFramebuffer = 0;
		}
		if (fb->id != 0) {
			glDeleteFramebuffers(1, &fb->id);
		}
		recycleResource(framebuffers, m_freeFramebuffers, framebuffer.id);
	}

	uint64_t OpenGLRHI::getNativeTextureId(TextureHandle tex) const
	{
		const GLTexture* t = getLive(textures, tex.id);
		return t ? t->id : 0;
	}

	void OpenGLRHI::updateTexture(
		TextureHandle tex,
		uint32_t x, uint32_t y,
		uint32_t w, uint32_t h,
		TextureFormat fmt,
		const void* data)
	{
		const GLTexture* t = getLive(textures, tex.id);
		if (!data || !t) {
			return;
		}
		
		GLenum target = ToOpenglTargetType(t->desc.dimension);

		glBindTexture(target, t->id);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTexSubImage2D(
			target,
			0,
			x, y,
			w, h,
			ToGLFormat(fmt),
			ToGLType(fmt),
			data);
	}


	ShaderModuleHandle OpenGLRHI::createShaderModule(const ShaderModuleDesc& desc)
	{
		GLenum glStage = toGLShaderStage(desc.stage);
		if (glStage == 0) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI: Unsupported shader stage");
			return {};
		}

		GLuint shader = glCreateShader(glStage);
		if (shader == 0) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI: glCreateShader failed");
			return {};
		}

		const char* src = desc.source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		// Проверка ошибок компиляции
		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			GLint logLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

			std::string log;
			log.resize(logLength);
			glGetShaderInfoLog(shader, logLength, nullptr, log.data());

			SFE::LogsModule::Logger::LOG_FATAL(false,
				"OpenGLRHI: Shader '%s' compilation failed:\n%s",
				desc.name.c_str(),
				log.c_str()
			);

			glDeleteShader(shader);
			return {};
		}

		// Создаем handle
		GLShaderModule mod{};
		mod.id = shader;
		mod.desc = desc;
		mod.live = true;

		ShaderModuleHandle handle;
		handle.id = storeResource(m_shaders, m_freeShaders, std::move(mod));
		return handle;
	}

	void OpenGLRHI::destroyShaderModule(ShaderModuleHandle handle)
	{
		GLShaderModule* mod = getLive(m_shaders, handle.id);
		if (!mod) {
			return;
		}

		if (mod->id != 0) {
			glDeleteShader(mod->id);
		}
		recycleResource(m_shaders, m_freeShaders, handle.id);
	}

	TextureHandle OpenGLRHI::createTexture(const TextureDesc& desc, const void* data)
	{
		GLuint tex;
		
		auto target = ToOpenglTargetType(desc.dimension);

		glGenTextures(1, &tex);
		glBindTexture(target, tex);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		if (desc.dimension == TextureDimension::Tex2D) {
			glTexImage2D(target,
				0,
				ToGLInternalFormat(desc.format),
				static_cast<GLsizei>(desc.width),
				static_cast<GLsizei>(desc.height),
				0,
				ToGLFormat(desc.format),
				ToGLType(desc.format),
				data);
		}
		else if (desc.dimension == TextureDimension::Cube) {
			for (int face = 0; face < 6; ++face) {
				glTexImage2D(
					GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
					0,
					ToGLInternalFormat(desc.format),
					static_cast<GLsizei>(desc.width),
					static_cast<GLsizei>(desc.height),
					0,
					ToGLFormat(desc.format),
					ToGLType(desc.format),
					nullptr
				);
			}
		}
		else if (desc.dimension == TextureDimension::Array) {
			const GLsizei layers = static_cast<GLsizei>(desc.depth > 1 ? desc.depth : desc.arrayLayers);
			const GLsizei levels = desc.mipLevels > 0 ? static_cast<GLsizei>(desc.mipLevels) : 1;
			glTexStorage3D(target, levels, static_cast<GLenum>(ToGLInternalFormat(desc.format)),
				static_cast<GLsizei>(desc.width),
				static_cast<GLsizei>(desc.height),
				layers);
			if (data) {
				glTexSubImage3D(target, 0, 0, 0, 0,
					static_cast<GLsizei>(desc.width),
					static_cast<GLsizei>(desc.height),
					layers,
					ToGLFormat(desc.format),
					ToGLType(desc.format),
					data);
			}
		}
		else {
			glTexImage3D(target,
				0,
				ToGLInternalFormat(desc.format),
				static_cast<GLsizei>(desc.width),
				static_cast<GLsizei>(desc.height),
				static_cast<GLsizei>(desc.depth),
				0,
				ToGLFormat(desc.format),
				ToGLType(desc.format),
				data);
		}

		if (desc.format == TextureFormat::DEPTH24_STENCIL8 || desc.format == TextureFormat::DEPTH32F_STENCIL8)
		{
			glTextureParameteri(tex, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
		}

		const GLint maxLevel = desc.mipLevels > 0 ? static_cast<GLint>(desc.mipLevels - 1) : 0;
		glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, maxLevel);
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, maxLevel > 0 ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindTexture(target, 0);

		GLTexture glt{};
		glt.id = tex;
		glt.desc = desc;
		glt.live = true;

		TextureHandle handle;
		handle.id = storeResource(textures, m_freeTextures, std::move(glt));
		return handle;
	}

	void OpenGLRHI::destroyTexture(TextureHandle texture)
	{
		GLTexture* t = getLive(textures, texture.id);
		if (!t) {
			return;
		}
		if (t->id != 0) {
			glDeleteTextures(1, &t->id);
		}
		recycleResource(textures, m_freeTextures, texture.id);
	}

	SamplerHandle OpenGLRHI::createSampler(const SamplerDesc& desc)
	{
		GLuint samplerId;
		glGenSamplers(1, &samplerId);
		if (samplerId == 0) {
			return {};
		}

		auto toGlFilter = [](FilterMode f, bool isMin) -> GLint
		{
			switch (f) {
			case FilterMode::Nearest:
				return isMin ? GL_NEAREST : GL_NEAREST;
			case FilterMode::Linear:
			default:
				return isMin ? GL_LINEAR : GL_LINEAR;
			}
		};

		auto toGlAddress = [](AddressMode m) -> GLint
		{
			switch (m) {
			case AddressMode::Repeat:				return GL_REPEAT;
			case AddressMode::ClampToEdge:			return GL_CLAMP_TO_EDGE;
			case AddressMode::MirrorRepeat:			return GL_MIRRORED_REPEAT;
			case AddressMode::ClampToBorder:		return GL_CLAMP_TO_BORDER;
			case AddressMode::MirrorClampToEdge:	return GL_MIRROR_CLAMP_TO_EDGE;
			default:								return GL_CLAMP_TO_EDGE;
			}
		};


	
					/*{BASE_LEVEL, baseLevel},

					{MAX_LEVEL, maxLevel},

					{SWIZZLE_R, static_cast<int>(swizzle.R)},
					{SWIZZLE_G, static_cast<int>(swizzle.G)},
					{SWIZZLE_B, static_cast<int>(swizzle.B)},
					{SWIZZLE_A, static_cast<int>(swizzle.A)},*/


		// filtering
		glSamplerParameteri(samplerId, GL_TEXTURE_MIN_FILTER, toGlFilter(desc.minFilter, true));
		glSamplerParameteri(samplerId, GL_TEXTURE_MAG_FILTER, toGlFilter(desc.magFilter, false));

		// addressing
		glSamplerParameteri(samplerId, GL_TEXTURE_WRAP_S, toGlAddress(desc.addressU));
		glSamplerParameteri(samplerId, GL_TEXTURE_WRAP_T, toGlAddress(desc.addressV));
		glSamplerParameteri(samplerId, GL_TEXTURE_WRAP_R, toGlAddress(desc.addressW));

		// comparison
		if (desc.compareOp == CompareOp::None) {
			glSamplerParameteri(samplerId, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}
		else {
			glSamplerParameteri(samplerId, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(samplerId, GL_TEXTURE_COMPARE_FUNC, ToGLCompareFunc(desc.compareOp));
		}


		// if (desc.enableAnisotropy) {
		//     glSamplerParameterf(handle.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, desc.maxAnisotropy);
		// }
		// glSamplerParameterf(handle.id, GL_TEXTURE_MIN_LOD, desc.minLod);
		// glSamplerParameterf(handle.id, GL_TEXTURE_MAX_LOD, desc.maxLod);
		// glSamplerParameterf(handle.id, GL_TEXTURE_LOD_BIAS, desc.mipLodBias);
		//
		// Если есть ClampToBorder и свой цвет бордера:
		// if (desc.addressU == AddressMode::ClampToBorder ||
		//     desc.addressV == AddressMode::ClampToBorder ||
		//     desc.addressW == AddressMode::ClampToBorder) {
		//     glSamplerParameterfv(handle.id, GL_TEXTURE_BORDER_COLOR, desc.borderColor.data());
		// }

		SamplerHandle handle;
		GLSampler gls{};
		gls.id = samplerId;
		gls.desc = desc;
		gls.live = true;
		handle.id = storeResource(samplers, m_freeSamplers, std::move(gls));
		return handle;
	}

	void OpenGLRHI::destroySampler(SamplerHandle sampler)
	{
		GLSampler* s = getLive(samplers, sampler.id);
		if (!s) {
			return;
		}
		if (s->id != 0) {
			glDeleteSamplers(1, &s->id);
		}
		recycleResource(samplers, m_freeSamplers, sampler.id);
	}
	static GLbitfield ToGLMask(BlitMask mask)
	{
		GLbitfield bits = 0;
		if (static_cast<uint32_t>(mask) & static_cast<uint32_t>(BlitMask::Color))
			bits |= GL_COLOR_BUFFER_BIT;
		if (static_cast<uint32_t>(mask) & static_cast<uint32_t>(BlitMask::Depth))
			bits |= GL_DEPTH_BUFFER_BIT;
		if (static_cast<uint32_t>(mask) & static_cast<uint32_t>(BlitMask::Stencil))
			bits |= GL_STENCIL_BUFFER_BIT;
		return bits;
	}

	static GLenum ToGLFilter(BlitFilter f)
	{
		switch (f) {
		case BlitFilter::Linear:  return GL_LINEAR;
		case BlitFilter::Nearest:
		default:                  return GL_NEAREST;
		}
	}

	void OpenGLRHI::cmdBlit(CommandListHandle cmd, FramebufferHandle src, FramebufferHandle dst, BlitMask mask, BlitFilter filter)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdBlit{ src, dst, mask, filter });
		}
	}

	void OpenGLRHI::execBlit(FramebufferHandle src, FramebufferHandle dst, BlitMask mask, BlitFilter filter)
	{
		auto resolveFb = [this](FramebufferHandle handle, int& w, int& h) -> GLuint {
			if (!handle) {
				auto* window = Engine::instance()->getWindow();
				w = window ? window->getScreenData().renderW : 0;
				h = window ? window->getScreenData().renderH : 0;
				return 0;
			}
			const auto* fb = getLive(framebuffers, handle.id);
			if (!fb) {
				w = 0;
				h = 0;
				return 0;
			}
			w = static_cast<int>(fb->desc.width);
			h = static_cast<int>(fb->desc.height);
			return fb->id;
		};

		int srcW = 0, srcH = 0, dstW = 0, dstH = 0;
		const GLuint srcId = resolveFb(src, srcW, srcH);
		const GLuint dstId = resolveFb(dst, dstW, dstH);
		if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
			return;
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, srcId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstId);
		glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, dstW, dstH, ToGLMask(mask), ToGLFilter(filter));
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	static GLbitfield ToGLMemoryBarrier(AccessMask access)
	{
		GLbitfield bits = 0;
		if (access & AccessMask::IndirectCommandRead) {
			bits |= GL_COMMAND_BARRIER_BIT;
		}
		if (access & AccessMask::IndexRead) {
			bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
		}
		if (access & AccessMask::VertexAttributeRead) {
			bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
		}
		if (access & AccessMask::UniformRead) {
			bits |= GL_UNIFORM_BARRIER_BIT;
		}
		if (access & AccessMask::ShaderRead) {
			bits |= GL_TEXTURE_FETCH_BARRIER_BIT
				| GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
				| GL_SHADER_STORAGE_BARRIER_BIT
				| GL_UNIFORM_BARRIER_BIT
				| GL_FRAMEBUFFER_BARRIER_BIT;
		}
		if (access & AccessMask::ShaderWrite) {
			bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
		}
		if ((access & AccessMask::ColorAttachmentRead) || (access & AccessMask::ColorAttachmentWrite)
			|| (access & AccessMask::DepthStencilAttachmentRead) || (access & AccessMask::DepthStencilAttachmentWrite)) {
			bits |= GL_FRAMEBUFFER_BARRIER_BIT;
		}
		if ((access & AccessMask::TransferRead) || (access & AccessMask::TransferWrite)) {
			bits |= GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT;
		}
		if ((access & AccessMask::HostRead) || (access & AccessMask::HostWrite)) {
			bits |= GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT;
		}
		return bits;
	}

	void OpenGLRHI::cmdPipelineBarrier(CommandListHandle cmd, const PipelineBarrier& barrier)
	{
		if (GLCommandList* list = recordingList(cmd)) {
			list->commands.emplace_back(CmdPipelineBarrier{ barrier });
		}
	}

	void OpenGLRHI::execPipelineBarrier(const PipelineBarrier& barrier)
	{
		const GLbitfield bits = ToGLMemoryBarrier(barrier.dstAccess);
		if (bits == 0) {
			return;
		}
		glMemoryBarrier(bits);
	}

	BufferHandle OpenGLRHI::createBuffer(const BufferDesc& desc, const void* initialData)
	{
		GLBuffer glb{};
		glb.desc = desc;
		glb.target = toGLBufferTarget(desc.usage);

		glGenBuffers(1, &glb.id);
		if (glb.id == 0)
		{
			BufferHandle invalid{};
			return invalid;
		}

		glBindBuffer(glb.target, glb.id);
		GLenum usage = toGLAccessFlags(desc.cpuAccess);
		glBufferData(glb.target, static_cast<GLsizeiptr>(desc.size), initialData, usage);
		glBindBuffer(glb.target, 0);

		glb.live = true;

		BufferHandle handle;
		handle.id = storeResource(m_buffers, m_freeBuffers, std::move(glb));
		return handle;
	}

	void OpenGLRHI::destroyBuffer(BufferHandle handle)
	{
		GLBuffer* glb = getLive(m_buffers, handle.id);
		if (!glb) {
			return;
		}

		if (glb->id != 0)
		{
			glDeleteBuffers(1, &glb->id);
		}
		recycleResource(m_buffers, m_freeBuffers, handle.id);
	}

	void OpenGLRHI::updateBuffer(BufferHandle handle, size_t offset, size_t size, const void* data)
	{
		execUpdateBuffer(handle, offset, size, data);
	}

	void OpenGLRHI::cmdUpdateBuffer(CommandListHandle cmd, BufferHandle handle, size_t offset, size_t size, const void* data)
	{
		GLCommandList* list = recordingList(cmd);
		if (!list || !data || size == 0) {
			return;
		}

		CmdUpdateBuffer rec;
		rec.handle = handle;
		rec.offset = offset;
		rec.data.resize(size);
		std::memcpy(rec.data.data(), data, size);
		list->commands.emplace_back(std::move(rec));
	}

	void OpenGLRHI::execUpdateBuffer(BufferHandle handle, size_t offset, size_t size, const void* data)
	{
		GLBuffer* glb = getLive(m_buffers, handle.id);
		if (!glb || glb->id == 0 || data == nullptr || size == 0) {
			return;
		}

		glNamedBufferSubData(glb->id,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(size),
			data);
	}

	void* OpenGLRHI::mapBuffer(BufferHandle handle, MapMode mode)
	{
		GLBuffer* glb = getLive(m_buffers, handle.id);
		if (!glb || glb->id == 0) {
			return nullptr;
		}

		glBindBuffer(glb->target, glb->id);

		GLbitfield flags = toGLMapFlags(mode, glb->desc.cpuAccess);

		void* ptr = glMapBufferRange(
			glb->target,
			0,
			static_cast<GLsizeiptr>(glb->desc.size),
			flags
		);

		// Можно оставить привязанным или отвязать — в зависимости от стиля
		glBindBuffer(glb->target, 0);

		return ptr;
	}

	void OpenGLRHI::unmapBuffer(BufferHandle handle)
	{
		GLBuffer* glb = getLive(m_buffers, handle.id);
		if (!glb || glb->id == 0) {
			return;
		}

		glBindBuffer(glb->target, glb->id);
		glUnmapBuffer(glb->target);
		glBindBuffer(glb->target, 0);
	}
}
