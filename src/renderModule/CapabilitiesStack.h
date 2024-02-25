#pragma once
#include <stack>

#include "core/Singleton.h"
#include "glad/glad.h"

namespace SFE::Render {

	enum Capability : unsigned {
		BLEND = GL_BLEND,													// If enabled, blend the computed fragment color values with the values in the color buffers.See glBlendFunc.

		COLOR_LOGIC_OP = GL_COLOR_LOGIC_OP,									// If enabled, apply the currently selected logical operation to the computed fragment color and color buffer values.See glLogicOp.

		CULL_FACE = GL_CULL_FACE,											// If enabled, cull polygons based on their winding in window coordinates.See glCullFace.

		DEBUG_OUTPUT = GL_DEBUG_OUTPUT,										// If enabled, debug messages are produced by a debug context.When disabled, the debug message log is silenced.Note that in a non - debug context, very few, if any messages might be produced, even when GL_DEBUG_OUTPUT is enabled.

		DEBUG_OUTPUT_SYNCHRONOUS = GL_DEBUG_OUTPUT_SYNCHRONOUS,				// If enabled, debug messages are produced synchronously by a debug context.If disabled, debug messages may be produced asynchronously.In particular, they may be delayed relative to the execution of GL commands, and the debug callback function may be called from a thread other than that in which the commands are executed.See glDebugMessageCallback.

		DEPTH_CLAMP = GL_DEPTH_CLAMP,										// If enabled, the −wc≤zc≤wc plane equation is ignored by view volume clipping(effectively, there is no near or far plane clipping).See glDepthRange.

		DEPTH_TEST = GL_DEPTH_TEST,											// If enabled, do depth comparisons and update the depth buffer.Note that even if the depth buffer exists and the depth mask is non - zero, the depth buffer is not updated if the depth test is disabled.See glDepthFunc and glDepthRange.

		DITHER = GL_DITHER,													// If enabled, dither color components or indices before they are written to the color buffer.

		FRAMEBUFFER_SRGB = GL_FRAMEBUFFER_SRGB,								// If enabled and the value of GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING for the framebuffer attachment corresponding to the destination buffer is GL_SRGB, the R, G, and B destination color values(after conversion from fixed - point to floating - point) are considered to be encoded for the sRGB color space and hence are linearized prior to their use in blending.

		LINE_SMOOTH = GL_LINE_SMOOTH,										// If enabled, draw lines with correct filtering.Otherwise, draw aliased lines.See glLineWidth.

		MULTISAMPLE = GL_MULTISAMPLE,										// If enabled, use multiple fragment samples in computing the final color of a pixel.See glSampleCoverage.

		POLYGON_OFFSET_FILL = GL_POLYGON_OFFSET_FILL,						// If enabled, and if the polygon is rendered in GL_FILL mode, an offset is added to depth values of a polygon's fragments before the depth comparison is performed. See glPolygonOffset.

		POLYGON_OFFSET_LINE = GL_POLYGON_OFFSET_LINE,						// If enabled, and if the polygon is rendered in GL_LINE mode, an offset is added to depth values of a polygon's fragments before the depth comparison is performed. See glPolygonOffset.

		POLYGON_OFFSET_POINT = GL_POLYGON_OFFSET_POINT,						// If enabled, an offset is added to depth values of a polygon's fragments before the depth comparison is performed, if the polygon is rendered in GL_POINT mode. See glPolygonOffset.

		POLYGON_SMOOTH = GL_POLYGON_SMOOTH,									// If enabled, draw polygons with proper filtering.Otherwise, draw aliased polygons.For correct antialiased polygons, an alpha buffer is needed and the polygons must be sorted front to back.

		PRIMITIVE_RESTART = GL_PRIMITIVE_RESTART,							// Enables primitive restarting.If enabled, any one of the draw commands which transfers a set of generic attribute array elements to the GL will restart the primitive when the index of the vertex is equal to the primitive restart index.See glPrimitiveRestartIndex.

		PRIMITIVE_RESTART_FIXED_INDEX = GL_PRIMITIVE_RESTART_FIXED_INDEX,	// Enables primitive restarting with a fixed index.If enabled, any one of the draw commands which transfers a set of generic attribute array elements to the GL will restart the primitive when the index of the vertex is equal to the fixed primitive index for the specified index type.The fixed index is equal to 2n−1 where n is equal to 8 for GL_UNSIGNED_BYTE, 16 for GL_UNSIGNED_SHORT and 32 for GL_UNSIGNED_INT.

		RASTERIZER_DISCARD = GL_RASTERIZER_DISCARD,							// If enabled, primitives are discarded after the optional transform feedback stage, but before rasterization.Furthermore, when enabled, glClear, glClearBufferData, glClearBufferSubData, glClearTexImage, and glClearTexSubImage are ignored.

		SAMPLE_ALPHA_TO_COVERAGE = GL_SAMPLE_ALPHA_TO_COVERAGE,				// If enabled, compute a temporary coverage value where each bit is determined by the alpha value at the corresponding sample location.The temporary coverage value is then ANDed with the fragment coverage value.

		SAMPLE_ALPHA_TO_ONE = GL_SAMPLE_ALPHA_TO_ONE,						// If enabled, each sample alpha value is replaced by the maximum representable alpha value.

		SAMPLE_COVERAGE = GL_SAMPLE_COVERAGE,								// If enabled, the fragment's coverage is ANDed with the temporary coverage value. If GL_SAMPLE_COVERAGE_INVERT is set to GL_TRUE, invert the coverage value. See glSampleCoverage.

		SAMPLE_SHADING = GL_SAMPLE_SHADING,									// If enabled, the active fragment shader is run once for each covered sample, or at fraction of this rate as determined by the current value of GL_MIN_SAMPLE_SHADING_VALUE.See glMinSampleShading.

		SAMPLE_MASK = GL_SAMPLE_MASK,										// If enabled, the sample coverage mask generated for a fragment during rasterization will be ANDed with the value of GL_SAMPLE_MASK_VALUE before shading occurs.See glSampleMaski.

		SCISSOR_TEST = GL_SCISSOR_TEST,										// If enabled, discard fragments that are outside the scissor rectangle.See glScissor.

		STENCIL_TEST = GL_STENCIL_TEST,										// If enabled, do stencil testing and update the stencil buffer.See glStencilFunc and glStencilOp.

		TEXTURE_CUBE_MAP_SEAMLESS = GL_TEXTURE_CUBE_MAP_SEAMLESS,			// If enabled, cubemap textures are sampled such that when linearly sampling from the border between two adjacent faces, texels from both faces are used to generate the final sample value.When disabled, texels from only a single face are used to construct the final sample value.

		PROGRAM_POINT_SIZE = GL_PROGRAM_POINT_SIZE,							// If enabled and a vertex or geometry shader is active, then the derived point size is taken from the(potentially clipped) shader builtin gl_PointSize and clamped to the implementation - dependent point size range.
	};


	//GL_CLIP_DISTANCE i
	// If enabled, clip geometry against user - defined half space i.


	struct CapabilityState {
		Capability capability;
		bool currentState = false;
		bool previousState = false;
	};

	struct CapabilitiesStack : Singleton<CapabilitiesStack> {//may be is should exist per opengl context, but now i have only one context todo
		static void push(const Capability& capability, bool state) {
			instance()->pushImpl(capability, state);
		}

		static void pop() {
			instance()->popImpl();
		}

		void pushImpl(Capability capability, bool state) {
			CapabilityState capState{ capability };
			capState.previousState = glIsEnabled(capability);
			capState.currentState = state;
			
			if (capState.previousState != capState.currentState) {
				if (capState.currentState) {
					glEnable(capability);
				}
				else {
					glDisable(capability);
				}
			}

			mStack.push(std::move(capState));
		}

		void popImpl() {
			const auto& capState = mStack.top();
			if (capState.previousState != capState.currentState) {
				if (capState.previousState) {
					glEnable(capState.capability);
				}
				else {
					glDisable(capState.capability);
				}
			}

			mStack.pop();
		}

	private:
		std::stack<CapabilityState> mStack;
	};
}

