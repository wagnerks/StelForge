#pragma once
#include <cassert>
#include <vector>

#include "Draw.h"
#include "glad/glad.h"


namespace SFE::GLW {
	struct Texture;

	enum RenderBuffer {
		NONE = GL_NONE, //No color buffers are written.

		// left and right buffer used for stereo rendering, for example for vr 
		FRONT_LEFT = GL_FRONT_LEFT, //Only the front left color buffer is written.

		FRONT_RIGHT = GL_FRONT_RIGHT, //Only the front right color buffer is written.

		BACK_LEFT = GL_BACK_LEFT, //Only the back left color buffer is written.

		BACK_RIGHT = GL_BACK_RIGHT,	//Only the back right color buffer is written.

		// front back buffer used for double buffering rendering, when front rendered on screen, and back rendering behind preparing data
		FRONT = GL_FRONT, //Only the front left and front right color buffers are written.If there is no front right color buffer, only the front left color buffer is written.

		BACK = GL_BACK, //Only the back left and back right color buffers are written.If there is no back right color buffer, only the back left color buffer is written.

		LEFT = GL_LEFT, //Only the front left and back left color buffers are written.If there is no back left color buffer, only the front left color buffer is written.

		RIGHT = GL_RIGHT, //Only the front right and back right color buffers are written.If there is no back right color buffer, only the front right color buffer is written.

		FRONT_AND_BACK = GL_FRONT_AND_BACK, //All the front and back color buffers(front left, front right, back left, back right) are written.If there are no back color buffers, only the front left and front right color buffers are written.If there are no right color buffers, only the front left and back left color buffers are written.If there are no right or back color buffers, only the front left color buffer is written.
	};

	enum class AttachmentType {
		DEPTH = GL_DEPTH_ATTACHMENT,
		STENCIL = GL_STENCIL_ATTACHMENT,
		DEPTH_STENCIL = GL_DEPTH_STENCIL_ATTACHMENT
	};
	

	constexpr void bindReadFramebuffer(unsigned id = 0) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
	}

	constexpr void bindDrawFramebuffer(unsigned id = 0) {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id); // write to default framebuffer
	}

	enum class BlitFilter {
		NEAREST = GL_NEAREST,
		LINEAR = GL_LINEAR
	};

	constexpr void blitFramebuffer(int srcX0, int srcY0, int srcW, int srcH, int dstX0, int dstY0, int dstW, int dstH, ColorBit mask, BlitFilter filter) {
		glBlitFramebuffer(srcX0, srcY0, srcW, srcH, dstX0, dstY0, dstW, dstH, static_cast<int>(mask), static_cast<int>(filter));
	}

	struct Framebuffer {
		Framebuffer(const Framebuffer& other) = delete;
		Framebuffer(Framebuffer&& other) noexcept = delete;
		Framebuffer& operator=(const Framebuffer& other) = delete;
		Framebuffer& operator=(Framebuffer&& other) noexcept = delete;

		Framebuffer();
		~Framebuffer();

		void bind() const;

		void addAttachment2D(unsigned attachment, Texture* texture);
		void addRenderbuffer(unsigned attachment, unsigned renderBuffer);
		void addRenderbuffer(AttachmentType attachment, unsigned renderBuffer);

		void finalize() const;
		void addAttachmentTexture(int attachment, unsigned texture);
		void addAttachmentTexture(int attachment, Texture* texture);

		void addAttachmentTexture(AttachmentType attachment, unsigned texture);
		void addAttachmentTexture(AttachmentType attachment, Texture* texture);

		static void bindFramebuffer(unsigned id = 0);
		static void bindDefaultFramebuffer();

		static void setReadBuffer(RenderBuffer buf);
		static void setDrawBuffer(RenderBuffer buf);

		unsigned id;

	private:
		std::vector<unsigned> attachments;
	};
}

