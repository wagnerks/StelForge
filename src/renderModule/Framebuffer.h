#pragma once
#include <cassert>
#include <vector>
#include "assetsModule/TextureHandler.h"


namespace SFE::Render {

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

	struct Framebuffer {
		Framebuffer();
		~Framebuffer();

		void bind() const;

		void addAttachment2D(unsigned attachment, AssetsModule::Texture* texture);
		void addRenderbuffer(unsigned attachment, unsigned renderBuffer);

		void finalize() const;
		void addAttachmentTexture(int attachment, unsigned texture);
		void addAttachmentTexture(int attachment, AssetsModule::Texture* texture);

		static void bindFramebuffer(unsigned id = 0);
		static void bindDefaultFramebuffer();

		static void setReadBuffer(RenderBuffer buf);
		static void setDrawBuffer(RenderBuffer buf);

		unsigned id;

	private:
		std::vector<unsigned> attachments;
	};
}

