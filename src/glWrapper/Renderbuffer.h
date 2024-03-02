#pragma once

#include "Texture.h"
#include "glad/glad.h"

namespace SFE::GLW {
	struct Renderbuffer {
		void generate() {
			glGenRenderbuffers(1, &id);
			
		}
		void bind() {
			glBindRenderbuffer(GL_RENDERBUFFER, id);
			
		}
		void storage(PixelFormat internalFormat, int width, int height) {
			glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
		}

		unsigned id;
	};
}