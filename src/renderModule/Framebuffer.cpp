#include "Framebuffer.h"
#include "glad/glad.h"
#include "logsModule/logger.h"

namespace SFE::Render {
	Framebuffer::Framebuffer() {
		glGenFramebuffers(1, &id);
	}

	Framebuffer::~Framebuffer() {
		glDeleteFramebuffers(1, &id);
	}

	void Framebuffer::finalize() const {
		glDrawBuffers(static_cast<int>(attachments.size()), attachments.data());

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
		}
	}

	void Framebuffer::addAttachmentTexture(int attachment, unsigned texture) {
		//https://stackoverflow.com/questions/17092075/what-is-the-point-of-the-textarget-parameter-for-glframebuffertexture
		if (attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT || attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
			glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, 0);
		}
		else {
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment, texture, 0);
			attachments.push_back(GL_COLOR_ATTACHMENT0 + attachment);
		}
	}

	void Framebuffer::addAttachmentTexture(int attachment, AssetsModule::Texture* texture) {
		addAttachmentTexture(attachment, texture->mId);
	}

	void Framebuffer::bindFramebuffer(unsigned id) {
		glBindFramebuffer(GL_FRAMEBUFFER, id);
	}

	void Framebuffer::bindDefaultFramebuffer() {
		bindFramebuffer(0);
	}

	void Framebuffer::setReadBuffer(RenderBuffer buf) {
		glReadBuffer(buf);
	}

	void Framebuffer::setDrawBuffer(RenderBuffer buf) {
		glDrawBuffer(buf);
	}

	void Framebuffer::bind() const {
		bindFramebuffer(id);
	}

	void Framebuffer::addAttachment2D(unsigned attachment, AssetsModule::Texture* texture) {
		if (attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT || attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, texture->mType, texture->mId, 0);
		}
		else {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment, texture->mType, texture->mId, 0);
			attachments.push_back(GL_COLOR_ATTACHMENT0 + attachment);
		}
	}

	void Framebuffer::addRenderbuffer(unsigned attachment, unsigned renderBuffer) {
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderBuffer);
	}
}
