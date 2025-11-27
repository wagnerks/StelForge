#include "OpenGLRHI.h"

#include "core/Engine.h"

#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Depth.h"
#include "glWrapper/Draw.h"
#include "glWrapper/Texture.h"
#include "glWrapper/ViewportStack.h"

#include "logsModule/logger.h"
#include "windowsModule/Window.h"

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
		case PrimitiveTopology::Points:   return GL_POINTS;
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

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, int value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}

		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}

		glUniform1i(gp.uniformLocations[name], value);
	}

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, float value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}
		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}
		glUniform1f(gp.uniformLocations[name], value);
	}

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, const Math::Mat4& value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}

		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}

		glUniformMatrix4fv(gp.uniformLocations[name], 1, false, value.data());
	}

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, const Math::Vec4& value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}
		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}
		glUniform4fv(gp.uniformLocations[name], 1, value.data());
	}

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, const Math::Vec3& value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}

		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}

		glUniform3fv(gp.uniformLocations[name], 1, value.data());
	}

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, const Math::Vec2& value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}
		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}
		glUniform2fv(gp.uniformLocations[name], 1, value.data());
	}

	void OpenGLRHI::draw(uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)
	{
		if (vertexCount == 0 || instanceCount == 0) {
			return;
		}

		if (instanceCount == 1) {
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

	void OpenGLRHI::drawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, IndexType indexType, int32_t vertexOffset)
	{
		if (indexCount == 0 || instanceCount == 0) {
			return;
		}
		
		const GLenum glIndexType = ToGLIndexType(indexType);
		const GLsizei indexSize = (indexType == IndexType::Uint16) ? sizeof(uint16_t) : sizeof(uint32_t);

		uintptr_t baseOffsetBytes = static_cast<uintptr_t>(m_currentIndexOffset);
		uintptr_t firstIndexBytes = static_cast<uintptr_t>(firstIndex) * indexSize;
		const void* indexOffset = reinterpret_cast<const void*>(baseOffsetBytes + firstIndexBytes);

		if (instanceCount == 1) {
			if (vertexOffset == 0) {
				glDrawElements(
					m_currentPrimitive,
					static_cast<GLsizei>(indexCount),
					glIndexType,
					indexOffset
				);
			}
			else {
				glDrawElementsBaseVertex(
					m_currentPrimitive,
					static_cast<GLsizei>(indexCount),
					glIndexType,
					indexOffset,
					vertexOffset
				);
			}
		}
		else {
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
	}

	void OpenGLRHI::setUniform(PipelineHandle handle, const std::string& name, bool value)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}
		GLGraphicsPipeline& gp = m_pipelines[handle.id];
		if (!gp.uniformLocations.contains(name)) {
			gp.uniformLocations[name] = glGetUniformLocation(gp.programId, name.c_str());
		}
		glUniform1i(gp.uniformLocations[name], value ? 1 : 0);
	}

	RenderPassHandle OpenGLRHI::createRenderPass(const RenderPassDesc& desc)
	{
		// В OpenGL рендер-пасс — чисто описание: LoadOp/StoreOp/clear-значения.
		// Никаких GL-объектов создавать не нужно.
		GLRenderPass rp{};
		rp.desc = desc;

		RenderPassHandle handle;
		handle.id = static_cast<uint32_t>(m_renderPasses.size());
		m_renderPasses.push_back(rp);

		return handle;
	}

	void OpenGLRHI::destroyRenderPass(RenderPassHandle renderPass)
	{
		if (renderPass.id >= m_renderPasses.size())
			return;

		m_renderPasses[renderPass.id].desc.colorAttachmentCount = 0;
	}

	void OpenGLRHI::beginRenderPass(const RenderPassBeginInfo& beginInfo)
	{
		if (beginInfo.renderPass.id >= m_renderPasses.size()) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::beginRenderPass: invalid RenderPass handle");
			return;
		}

		if (beginInfo.framebuffer.id >= framebuffers.size()) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::beginRenderPass: invalid Framebuffer handle");
			return;
		}

		const GLRenderPass& rp = m_renderPasses[beginInfo.renderPass.id];
		const GLFramebuffer& fb = framebuffers[beginInfo.framebuffer.id];

		// Сохраняем предыдущий FBO, чтобы вернуть в endRenderPass
		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
		m_prevFramebuffer = static_cast<GLuint>(prevFbo);

		// Биндим целевой FBO
		glBindFramebuffer(GL_FRAMEBUFFER, fb.id);

		// Вычисляем область
		int x = beginInfo.x;
		int y = beginInfo.y;
		int w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(fb.desc.width);
		int h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(fb.desc.height);

		// Через твой setViewport, чтобы всё шло через один путь
		ViewportDesc vp{};
		vp.x = x;
		vp.y = y;
		vp.width = w;
		vp.height = h;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;

		glViewport(vp.x, vp.y, vp.width, vp.height);
		glDepthRange(vp.minDepth, vp.maxDepth);

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

	void OpenGLRHI::endRenderPass()
	{
		// В Vulkan тут было бы vkCmdEndRenderPass.
		// В OpenGL достаточно восстановить предыдущий framebuffer.

		glBindFramebuffer(GL_FRAMEBUFFER, m_prevFramebuffer);
		m_prevFramebuffer = 0;
	}

	CommandListHandle OpenGLRHI::beginCommandList(CommandListType type)
	{
		// В OpenGL нет командных листов, возвращаем пустой хэндл
		return CommandListHandle{};
	}

	void OpenGLRHI::endCommandList(CommandListHandle cmd)
	{
		// В OpenGL нет командных листов, ничего не делаем
	}

	void OpenGLRHI::submit(const CommandListHandle* lists, uint32_t count)
	{
		// В OpenGL нет командных листов, ничего не делаем
	}

	void OpenGLRHI::present()
	{
		if (auto window = Engine::instance()->getWindow()) {
			window->swapBuffers();
		}
	}

	void OpenGLRHI::cmdBeginRenderPass(CommandListHandle cmd, const RenderPassBeginInfo& beginInfo)
	{
		if (beginInfo.renderPass.id >= m_renderPasses.size()) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "OpenGLRHI::beginRenderPass: invalid RenderPass handle");
			return;
		}

		const GLRenderPass& rp = m_renderPasses[beginInfo.renderPass.id];
		

		// Сохраняем предыдущий FBO, чтобы вернуть в endRenderPass
		GLint prevFbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
		m_prevFramebuffer = static_cast<GLuint>(prevFbo);
		int w = 0;
		int h = 0;
		if (beginInfo.framebuffer.id >= framebuffers.size()) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			auto& screen = Engine::instance()->getWindow()->getScreenData();
			w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(screen.width);
			h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(screen.height);
		}
		else {
			const GLFramebuffer& fb = framebuffers[beginInfo.framebuffer.id];
			glBindFramebuffer(GL_FRAMEBUFFER, fb.id);
			w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(fb.desc.width);
			h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(fb.desc.height);
		}


		// Вычисляем область
		int x = beginInfo.x;
		int y = beginInfo.y;
		//int w = beginInfo.width > 0 ? beginInfo.width : static_cast<int>(fb.desc.width);
		//int h = beginInfo.height > 0 ? beginInfo.height : static_cast<int>(fb.desc.height);

		// Через твой setViewport, чтобы всё шло через один путь
		ViewportDesc vp{};
		vp.x = x;
		vp.y = y;
		vp.width = w;
		vp.height = h;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;

		glViewport(vp.x, vp.y, vp.width, vp.height);
		glDepthRange(vp.minDepth, vp.maxDepth);

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

	void OpenGLRHI::cmdEndRenderPass(CommandListHandle cmd)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_prevFramebuffer);
		m_prevFramebuffer = 0;
	}

	void OpenGLRHI::cmdBindPipeline(CommandListHandle cmd, PipelineHandle pipeline)
	{
		if (pipeline.id >= m_pipelines.size()) {
			return;
		}

		const GLGraphicsPipeline& gp = m_pipelines[pipeline.id];

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
		if (m_currentPipeline.id >= m_pipelines.size())
			return;
		if (handle.id >= m_buffers.size())
			return;

		const auto& gp = m_pipelines[m_currentPipeline.id];
		const auto& buf = m_buffers[handle.id];

		// защитимся от кривого slot
		if (slot >= gp.desc.layout.bindings.size())
			return;

		SFE::LogsModule::Logger::LOG_FATAL(buf.desc.usage == BufferUsage::Vertex, "WRONG BUFFER TYPE BINDING");
		const auto& bindingDesc = gp.desc.layout.bindings[slot];
		const GLsizei stride = static_cast<GLsizei>(bindingDesc.stride);
		glVertexArrayVertexBuffer(
			gp.vaoId,
			bindingDesc.binding,                          // vertex binding
			buf.id,                        // VBO
			static_cast<GLintptr>(offset), // byte offset
			stride
		);
		glBindBuffer(buf.target, buf.id);
	}

	void OpenGLRHI::cmdBindIndexBuffer(CommandListHandle cmd, BufferHandle handle, IndexType type, uint32_t offset)
	{
		if (m_currentPipeline.id >= m_pipelines.size() || handle.id >= m_buffers.size()) {
			m_currentIndexType = type;
			m_currentIndexOffset = 0;
			return;
		}

		const auto& gp = m_pipelines[m_currentPipeline.id];
		const auto& buf = m_buffers[handle.id];
		SFE::LogsModule::Logger::LOG_FATAL(buf.desc.usage == BufferUsage::Index, "WRONG BUFFER TYPE BINDING");
		glVertexArrayElementBuffer(gp.vaoId, buf.id);
		glBindBuffer(buf.target, buf.id);
		m_currentIndexType = type;
		m_currentIndexOffset = offset;
	}

	void OpenGLRHI::cmdDraw(CommandListHandle cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		// В OpenGL игнорируем cmd и firstInstance — у нас нет baseInstance.
		// Просто вызываем уже существующий путь.
		draw(vertexCount, firstVertex, instanceCount);
	}

	void OpenGLRHI::cmdDrawIndexed(CommandListHandle cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		// Аналогично: cmd и firstInstance игнорируем.
		// Тип индекса берём из m_currentIndexType (он выставляется в cmdBindIndexBuffer).
		drawIndexed(indexCount, firstIndex, instanceCount, m_currentIndexType, vertexOffset);
	}

	void OpenGLRHI::cmdPushConstants(CommandListHandle, PipelineHandle pipeline, ShaderStage, const void* data, uint32_t size)
	{
		// emulation push constant in GL via UBO
		if (!m_pushConstantUBO) {
			glGenBuffers(1, &m_pushConstantUBO);
			glBindBuffer(GL_UNIFORM_BUFFER, m_pushConstantUBO);
			glBufferData(GL_UNIFORM_BUFFER, 256, nullptr, GL_DYNAMIC_DRAW);
		}

		glBindBuffer(GL_UNIFORM_BUFFER, m_pushConstantUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);

		glBindBufferBase(GL_UNIFORM_BUFFER, m_pushConstantPos, m_pushConstantUBO);
	}

	DescriptorSetLayoutHandle OpenGLRHI::createDescriptorSetLayout(
		const DescriptorSetLayoutDesc& desc)
	{
		DescriptorSetLayoutHandle h;
		h.id = static_cast<uint32_t>(m_descriptorSetLayouts.size());
		m_descriptorSetLayouts.push_back(desc);
		return h;
	}


	void OpenGLRHI::destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout)
	{

	}

	DescriptorSetHandle OpenGLRHI::allocateDescriptorSet(DescriptorSetLayoutHandle layout)
	{
		if (layout.id >= m_descriptorSetLayouts.size())
			return {};

		GLDescriptorSet set{};
		set.layout = m_descriptorSetLayouts[layout.id];

		// подготовим слоты под биндинги
		uint32_t maxBinding = 0;
		for (auto& b : set.layout.bindings)
			maxBinding = std::max(maxBinding, b.binding);

		set.bindings.resize(maxBinding + 1);
		for (auto& b : set.layout.bindings)
			set.bindings[b.binding].type = b.type;

		DescriptorSetHandle h;
		h.id = static_cast<uint32_t>(m_descriptorSets.size());
		m_descriptorSets.push_back(std::move(set));
		return h;
	}


	void OpenGLRHI::freeDescriptorSet(DescriptorSetHandle set)
	{

	}

	void OpenGLRHI::updateDescriptorSets(const std::vector<WriteDescriptor>& writes)
	{
		for (const auto& w : writes)
		{
			if (w.set.id >= m_descriptorSets.size())
				continue;

			GLDescriptorSet& set = m_descriptorSets[w.set.id];
			if (w.binding >= set.bindings.size())
				continue;

			auto& b = set.bindings[w.binding];
			b.type = w.type;

			switch (w.type)
			{
			case DescriptorType::UniformBuffer:
			case DescriptorType::StorageBuffer:
				b.buffers = w.buffers;
				break;

			case DescriptorType::CombinedImageSampler:
				b.images = w.images;
				break;
			}
		}
	}


	void OpenGLRHI::cmdBindDescriptorSets(CommandListHandle cmd, PipelineHandle layout, uint32_t firstSet, const DescriptorSetHandle* sets, uint32_t setCount)
	{
		// Для OpenGL считаем:
		// texture unit = binding (пока без извращений)
		// UBO/SSBO = glBindBufferBase(binding)

		for (uint32_t i = 0; i < setCount; ++i)
		{
			const DescriptorSetHandle h = sets[i];
			if (h.id >= m_descriptorSets.size())
				continue;

			const GLDescriptorSet& set = m_descriptorSets[h.id];

			for (uint32_t binding = 0; binding < set.bindings.size(); ++binding)
			{
				const auto& b = set.bindings[binding];

				switch (b.type)
				{
				case DescriptorType::UniformBuffer:
					for (const auto& bufInfo : b.buffers)
					{
						if (bufInfo.buffer.id >= m_buffers.size())
							continue;
						const auto& glb = m_buffers[bufInfo.buffer.id];
						glBindBufferBase(GL_UNIFORM_BUFFER, binding, glb.id);
					}
					break;

				case DescriptorType::StorageBuffer:
					for (const auto& bufInfo : b.buffers)
					{
						if (bufInfo.buffer.id >= m_buffers.size())
							continue;
						const auto& glb = m_buffers[bufInfo.buffer.id];
						glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, glb.id);
					}
					break;

				case DescriptorType::CombinedImageSampler:
					if (!b.images.empty())
					{
						const auto& img = b.images[0];
						if (img.texture.id < textures.size())
						{
							const auto& t = textures[img.texture.id];

							glActiveTexture(GL_TEXTURE0 + binding);
							glBindTexture(ToOpenglTargetType(t.desc.dimension), t.id);

							if (img.sampler.id < samplers.size())
								glBindSampler(binding, samplers[img.sampler.id].id);
						}
					}
					break;
				}
			}
		}
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
			if (h.id == UINT32_MAX || h.id >= m_shaders.size())
				return;
			const GLShaderModule& mod = m_shaders[h.id];
			if (mod.id != 0) {
				glAttachShader(program, mod.id);
			}
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

		// можно сразу отвязать шейдеры
		auto detachIfValid = [&](ShaderModuleHandle h)
		{
			if (h.id == UINT32_MAX || h.id >= m_shaders.size())
				return;
			const GLShaderModule& mod = m_shaders[h.id];
			if (mod.id != 0) {
				glDetachShader(program, mod.id);
			}
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
		handle.id = static_cast<uint32_t>(m_pipelines.size());
		m_pipelines.push_back(gp);

		return handle;
	}

	void OpenGLRHI::bindPipeline(PipelineHandle handle)
	{
		if (handle.id >= m_pipelines.size()) {
			return;
		}

		const GLGraphicsPipeline& gp = m_pipelines[handle.id];

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

		m_currentPipeline = handle;
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
		auto window = Engine::instance()->getWindow();
		if (!window) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Try to initialize render without context, forgot to call createWindow?");
			return;
		}
		InitGLDebug();

		SFE::GLW::ViewportStack::size = { window->getScreenData().renderW, window->getScreenData().renderH };
		

		SFE::GLW::setClearColor(0.f, 0.f, 0.f);
		
		GLW::CapabilitiesStack<SFE::GLW::CULL_FACE>::push(true);
		SFE::GLW::CapabilitiesStack<SFE::GLW::DEPTH_TEST>::push(true);
		SFE::GLW::DepthFuncStack::push(SFE::GLW::DepthFunc::LESS);

		SFE::GLW::setDepthDistance(window->getScreenData().far);
		
	}

	void OpenGLRHI::cleanup() {}

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
			const auto& tex = textures[att.texture.id];

			GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
			drawBuffers.push_back(attachment);

			switch (tex.desc.dimension) {
				case TextureDimension::Tex2D:
					glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex.id, static_cast<GLint>(att.mipLevel));
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
			const auto& tex = textures[att.texture.id];

			switch (tex.desc.dimension) {
			case TextureDimension::Tex2D:
				glFramebufferTexture2D(GL_FRAMEBUFFER, ToGLAttachmentFormat(tex.desc.format), GL_TEXTURE_2D, tex.id, static_cast<GLint>(att.mipLevel));
				break;
			case TextureDimension::Array:
			case TextureDimension::Tex3D:
				glFramebufferTextureLayer(GL_FRAMEBUFFER,
					ToGLAttachmentFormat(tex.desc.format),
					tex.id,
					static_cast<GLint>(att.mipLevel),
					static_cast<GLint>(att.layer));
				break;
			default:;
			}
		}

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Failed to create framebuffer object!");

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &fbo);
			return {};
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		FramebufferHandle handle;
		handle.id = static_cast<uint32_t>(framebuffers.size());
		framebuffers.push_back(GLFramebuffer{ fbo, desc });

		return handle;
	}

	void OpenGLRHI::destroyFramebuffer(FramebufferHandle framebuffer)
	{
	}

	void OpenGLRHI::setTextureSubImage2D(
		TextureHandle tex,
		uint32_t x, uint32_t y,
		uint32_t w, uint32_t h,
		TextureFormat fmt,
		const void* data)
	{
		if (!data || tex.id >= textures.size()) {
			return;
		}
		const auto& t = textures[tex.id];
		
		GLenum target = ToOpenglTargetType(t.desc.dimension);

		glBindTexture(target, t.id);

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
		ShaderModuleHandle handle;
		handle.id = static_cast<uint32_t>(m_shaders.size());

		// Сохраняем модуль
		m_shaders.push_back(GLShaderModule{
			shader,
			desc
			});

		return handle;
	}

	void OpenGLRHI::destroyShaderModule(ShaderModuleHandle handle)
	{
		if (handle.id >= m_shaders.size())
			return;

		GLShaderModule& mod = m_shaders[handle.id];

		if (mod.id != 0) {
			glDeleteShader(mod.id);
			mod.id = 0;
		}
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

		glBindTexture(target, 0);

		TextureHandle handle;
		handle.id = static_cast<uint32_t>(textures.size());
		textures.push_back(GLTexture{ tex, desc });

		return handle;
	}

	void OpenGLRHI::destroyTexture(TextureHandle texture)
	{
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
		handle.id = static_cast<uint32_t>(samplers.size());
		samplers.push_back(GLSampler{ samplerId, desc });

		return handle;
	}

	void OpenGLRHI::destroySampler(SamplerHandle sampler)
	{
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

	void OpenGLRHI::blitFramebuffer(
		FramebufferHandle src,
		FramebufferHandle dst,
		int srcX0, int srcY0, int srcX1, int srcY1,
		int dstX0, int dstY0, int dstX1, int dstY1,
		BlitMask mask,
		BlitFilter filter)
	{
		GLuint srcId = 0;
		if (src.id != UINT32_MAX)
			srcId = framebuffers[src.id].id;

		GLuint dstId = 0;
		if (dst.id != UINT32_MAX)
			dstId = framebuffers[dst.id].id;

		glBindFramebuffer(GL_READ_FRAMEBUFFER, srcId);
		//glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstId);

		glBlitFramebuffer(
			srcX0, srcY0, srcX1, srcY1,
			dstX0, dstY0, dstX1, dstY1,
			ToGLMask(mask),
			ToGLFilter(filter)
		);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, dstId);
	}
	void OpenGLRHI::bindTexture(uint32_t slot, TextureHandle tex, SamplerHandle sampler)
	{
		GLuint glTex = 0;
		GLenum target = GL_TEXTURE_2D;

		if (tex.id < textures.size()) {
			auto& t = textures[tex.id];
			glTex = t.id;
			target = ToOpenglTargetType(t.desc.dimension);
		}

		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(target, glTex); 

		GLuint glSampler = 0;
		if (sampler.id < samplers.size()) {
			glSampler = samplers[sampler.id].id;
		}

		glBindSampler(slot, glSampler);
	}

	void OpenGLRHI::bindFramebuffer(FramebufferHandle framebuffer)
	{
		if (framebuffer.id >= framebuffers.size()) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}
		const auto& fbo = framebuffers[framebuffer.id];
		glBindFramebuffer(GL_FRAMEBUFFER, fbo.id);
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

		BufferHandle handle;
		handle.id = static_cast<uint32_t>(m_buffers.size());
		m_buffers.push_back(glb);

		// при желании можешь повесить debug label через KHR_debug

		return handle;
	}

	void OpenGLRHI::destroyBuffer(BufferHandle handle)
	{
		if (handle.id >= m_buffers.size()) {
			return;
		}

		GLBuffer& glb = m_buffers[handle.id];
		if (glb.id != 0)
		{
			glDeleteBuffers(1, &glb.id);
			glb.id = 0;
		}
	}

	void OpenGLRHI::updateBuffer(BufferHandle handle, size_t offset, size_t size, const void* data)
	{
		if (handle.id >= m_buffers.size()) {
			return;
		}

		GLBuffer& glb = m_buffers[handle.id];
		if (glb.id == 0 || data == nullptr || size == 0) {
			return;
		}

		glBindBuffer(glb.target, glb.id);
		glBufferSubData(glb.target,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(size),
			data);
		glBindBuffer(glb.target, 0);
	}

	void* OpenGLRHI::mapBuffer(BufferHandle handle, MapMode mode)
	{
		if (handle.id >= m_buffers.size()) {
			return nullptr;
		}

		GLBuffer& glb = m_buffers[handle.id];
		if (glb.id == 0) {
			return nullptr;
		}

		glBindBuffer(glb.target, glb.id);

		GLbitfield flags = toGLMapFlags(mode, glb.desc.cpuAccess);

		void* ptr = glMapBufferRange(
			glb.target,
			0,
			static_cast<GLsizeiptr>(glb.desc.size),
			flags
		);

		// Можно оставить привязанным или отвязать — в зависимости от стиля
		glBindBuffer(glb.target, 0);

		return ptr;
	}

	void OpenGLRHI::unmapBuffer(BufferHandle handle)
	{
		if (handle.id >= m_buffers.size()) {
			return;
		}

		GLBuffer& glb = m_buffers[handle.id];
		if (glb.id == 0) {
			return;
		}

		glBindBuffer(glb.target, glb.id);
		glUnmapBuffer(glb.target);
		glBindBuffer(glb.target, 0);
	}

	void OpenGLRHI::bindVertexBuffer(uint32_t slot, BufferHandle handle, uint32_t offset)
	{
		if (m_currentPipeline.id >= m_pipelines.size())
			return;
		if (handle.id >= m_buffers.size())
			return;

		const auto& gp = m_pipelines[m_currentPipeline.id];
		const auto& buf = m_buffers[handle.id];

		// защитимся от кривого slot
		if (slot >= gp.desc.layout.bindings.size())
			return;

		SFE::LogsModule::Logger::LOG_FATAL(buf.desc.usage == BufferUsage::Vertex, "WRONG BUFFER TYPE BINDING");
		const auto& bindingDesc = gp.desc.layout.bindings[slot];
		const GLsizei stride = static_cast<GLsizei>(bindingDesc.stride);
		glVertexArrayVertexBuffer(
			gp.vaoId,
			bindingDesc.binding,                          // vertex binding
			buf.id,                        // VBO
			static_cast<GLintptr>(offset), // byte offset
			stride
		);
		glBindBuffer(buf.target, buf.id);
	}

	void OpenGLRHI::bindIndexBuffer(BufferHandle handle, IndexType type, uint32_t offset)
	{
		if (m_currentPipeline.id >= m_pipelines.size() || handle.id >= m_buffers.size()) {
			m_currentIndexType = type;
			m_currentIndexOffset = 0;
			return;
		}

		const auto& gp = m_pipelines[m_currentPipeline.id];
		const auto& buf = m_buffers[handle.id];
		SFE::LogsModule::Logger::LOG_FATAL(buf.desc.usage == BufferUsage::Index, "WRONG BUFFER TYPE BINDING");
		glVertexArrayElementBuffer(gp.vaoId, buf.id);
		glBindBuffer(buf.target, buf.id);
		m_currentIndexType = type;
		m_currentIndexOffset = offset;
	}

	void OpenGLRHI::bindUniformBuffer(uint32_t binding, BufferHandle handle)
	{
		if (handle.id >= m_buffers.size()) {
			return;
		}

		const GLBuffer& glb = m_buffers[handle.id];
		if (glb.id == 0) {
			return;
		}
		SFE::LogsModule::Logger::LOG_FATAL(glb.desc.usage == BufferUsage::Uniform, "WRONG BUFFER TYPE BINDING");
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, glb.id);
	}

	void OpenGLRHI::bindStorageBuffer(uint32_t binding, BufferHandle handle)
	{
		if (handle.id >= m_buffers.size()) {
			return;
		}

		const GLBuffer& glb = m_buffers[handle.id];
		if (glb.id == 0) {
			return;
		}
		SFE::LogsModule::Logger::LOG_FATAL(glb.desc.usage == BufferUsage::Storage, "WRONG BUFFER TYPE BINDING");
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, glb.id);
	}
}
