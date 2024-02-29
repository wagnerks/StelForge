#pragma once

#include <functional>

#include "glad/glad.h"

namespace SFE::GLW {

	constexpr int TextureSlot(int slot) {
		return GL_TEXTURE0 + slot;
	}

	enum PixelDataType : unsigned {
		UNSIGNED_BYTE = GL_UNSIGNED_BYTE,
		BYTE = GL_BYTE,
		UNSIGNED_SHORT = GL_UNSIGNED_SHORT,
		SHORT = GL_SHORT,
		UNSIGNED_INT = GL_UNSIGNED_INT,
		INT = GL_INT,
		HALF_FLOAT = GL_HALF_FLOAT,
		FLOAT = GL_FLOAT,
		UNSIGNED_BYTE_3_3_2 = GL_UNSIGNED_BYTE_3_3_2,
		UNSIGNED_BYTE_2_3_3_REV = GL_UNSIGNED_BYTE_2_3_3_REV,
		UNSIGNED_SHORT_5_6_5 = GL_UNSIGNED_SHORT_5_6_5,
		UNSIGNED_SHORT_5_6_5_REV = GL_UNSIGNED_SHORT_5_6_5_REV,
		UNSIGNED_SHORT_4_4_4_4 = GL_UNSIGNED_SHORT_4_4_4_4,
		UNSIGNED_SHORT_4_4_4_4_REV = GL_UNSIGNED_SHORT_4_4_4_4_REV,
		UNSIGNED_SHORT_5_5_5_1 = GL_UNSIGNED_SHORT_5_5_5_1,
		UNSIGNED_SHORT_1_5_5_5_REV = GL_UNSIGNED_SHORT_1_5_5_5_REV,
		UNSIGNED_INT_8_8_8_8 = GL_UNSIGNED_INT_8_8_8_8,
		UNSIGNED_INT_8_8_8_8_REV = GL_UNSIGNED_INT_8_8_8_8_REV,
		UNSIGNED_INT_10_10_10_2 = GL_UNSIGNED_INT_10_10_10_2,
		UNSIGNED_INT_2_10_10_10_REV = GL_UNSIGNED_INT_2_10_10_10_REV
	};

	enum PixelFormat : unsigned {
		R8 = GL_R8,
		R8_SNORM = GL_R8_SNORM,
		R16 = GL_R16,
		R16_SNORM = GL_R16_SNORM,
		RG8 = GL_RG8,
		RG8_SNORM = GL_RG8_SNORM,
		RG16 = GL_RG16,
		RG16_SNORM = GL_RG16_SNORM,
		R3_G3_B2 = GL_R3_G3_B2,
		RGB4 = GL_RGB4,
		RGB5 = GL_RGB5,
		RGB8 = GL_RGB8,
		RGB8_SNORM = GL_RGB8_SNORM,
		RGB10 = GL_RGB10,
		RGB12 = GL_RGB12,
		RGB16_SNORM = GL_RGB16_SNORM,
		RGBA2 = GL_RGBA2,
		RGBA4 = GL_RGBA4,
		RGB5_A1 = GL_RGB5_A1,
		RGBA8 = GL_RGBA8,
		RGBA8_SNORM = GL_RGBA8_SNORM,
		RGB10_A2 = GL_RGB10_A2,
		RGB10_A2UI = GL_RGB10_A2UI,
		RGBA12 = GL_RGBA12,
		RGBA16 = GL_RGBA16,
		SRGB8 = GL_SRGB8,
		SRGB8_ALPHA8 = GL_SRGB8_ALPHA8,
		R16F = GL_R16F,
		RG16F = GL_RG16F,
		RGB16F = GL_RGB16F,
		RGBA16F = GL_RGBA16F,
		R32F = GL_R32F,
		RG32F = GL_RG32F,
		RGB32F = GL_RGB32F,
		RGBA32F = GL_RGBA32F,
		R11F_G11F_B10F = GL_R11F_G11F_B10F,
		RGB9_E5 = GL_RGB9_E5,
		R8I = GL_R8I,
		R8UI = GL_R8UI,
		R16I = GL_R16I,
		R16UI = GL_R16UI,
		R32I = GL_R32I,
		R32UI = GL_R32UI,
		RG8I = GL_RG8I,
		RG8UI = GL_RG8UI,
		RG16I = GL_RG16I,
		RG16UI = GL_RG16UI,
		RG32I = GL_RG32I,
		RG32UI = GL_RG32UI,
		RGB8I = GL_RGB8I,
		RGB8UI = GL_RGB8UI,
		RGB16I = GL_RGB16I,
		RGB16UI = GL_RGB16UI,
		RGB32I = GL_RGB32I,
		RGB32UI = GL_RGB32UI,
		RGBA8I = GL_RGBA8I,
		RGBA8UI = GL_RGBA8UI,
		RGBA16I = GL_RGBA16I,
		RGBA16UI = GL_RGBA16UI,
		RGBA32I = GL_RGBA32I,
		RGBA32UI = GL_RGBA32UI,
		DEPTH_COMPONENT32F = GL_DEPTH_COMPONENT32F,
		DEPTH_COMPONENT16 = GL_DEPTH_COMPONENT16,
		DEPTH_COMPONENT24 = GL_DEPTH_COMPONENT24,
		DEPTH_COMPONENT32 = GL_DEPTH_COMPONENT32,
	};

	enum TextureFormat : unsigned {
		RED = GL_RED,
		RG = GL_RG,
		RGB = GL_RGB,
		BGR = GL_BGR,
		RGBA = GL_RGBA,
		BGRA = GL_BGRA,
		RED_INTEGER = GL_RED_INTEGER,
		RG_INTEGER = GL_RG_INTEGER,
		RGB_INTEGER = GL_RGB_INTEGER,
		BGR_INTEGER = GL_BGR_INTEGER,
		RGBA_INTEGER = GL_RGBA_INTEGER,
		BGRA_INTEGER = GL_BGRA_INTEGER,
		STENCIL_INDEX = GL_STENCIL_INDEX,
		DEPTH_COMPONENT = GL_DEPTH_COMPONENT,
		DEPTH_STENCIL = GL_DEPTH_STENCIL
	};

	enum class CompareFunc {
		NEVER = GL_NEVER, //Never passes.
		LESS = GL_LESS, // result = {1.00.0  r < Dtr >= Dt
		EQUAL = GL_EQUAL, // result = {1.00.0  r = Dtr≠Dt
		LEQUAL = GL_LEQUAL, // result = { 1.00.0  r <= Dtr > Dt
		GREATER = GL_GREATER, // result = {1.00.0  r > Dtr <= Dt
		NOTEQUAL = GL_NOTEQUAL, // result = {1.00.0  r≠Dtr = Dt
		GEQUAL = GL_GEQUAL, // result = {1.00.0  r >= Dtr < Dt
		ALWAYS = GL_ALWAYS, // result = 1.0
	};

	enum class DepthStencilTextureMode {
		DEPTH_COMPONENT = GL_DEPTH_COMPONENT, // reads from depth - stencil format textures will return the depth component of the texel in Rt and the stencil component will be discarded
		STENCIL_INDEX = GL_STENCIL_INDEX //the stencil component is returned in Rt and the depth component is discarded
	};

	enum class TextureCompareMode {
		NONE = GL_NONE, //Specifies that the red channel should be assigned the appropriate value from the currently bound depth texture.
		COMPARE_REF_TO_TEXTURE = GL_COMPARE_REF_TO_TEXTURE, //Specifies that the interpolated and clamped r texture coordinate should be compared to the value in the currently bound depth texture.
		//The result of the comparison is assigned to the red channel.
	};
	enum class TextureMinFilter {
		NEAREST = GL_NEAREST, //Returns the value of the texture element that is nearest (in Manhattan distance) to the specified texture coordinates.

		LINEAR = GL_LINEAR, //Returns the weighted average of the four texture elements that are closest to the specified texture coordinates. These can include items wrapped or repeated from other parts of a texture, depending on the values of GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T, and on the exact mapping.

		NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST, //Chooses the mipmap that most closely matches the size of the pixel being textured and uses the GL_NEAREST criterion (the texture element closest to the specified texture coordinates) to produce a texture value.

		LINEAR_MIPMAP_NEAREST = GL_LINEAR_MIPMAP_NEAREST, //Chooses the mipmap that most closely matches the size of the pixel being textured and uses the GL_LINEAR criterion (a weighted average of the four texture elements that are closest to the specified texture coordinates) to produce a texture value.

		NEAREST_MIPMAP_LINEAR = GL_NEAREST_MIPMAP_LINEAR, //Chooses the two mipmaps that most closely match the size of the pixel being textured and uses the GL_NEAREST criterion (the texture element closest to the specified texture coordinates ) to produce a texture value from each mipmap. The final texture value is a weighted average of those two values.

		LINEAR_MIPMAP_LINEAR = GL_LINEAR_MIPMAP_LINEAR //Chooses the two mipmaps that most closely match the size of the pixel being textured and uses the GL_LINEAR criterion (a weighted average of the texture elements that are closest to the specified texture coordinates) to produce a texture value from each mipmap. The final texture value is a weighted average of those two values.
	};

	enum class TextureMagFilter {
		NEAREST = GL_NEAREST, //Returns the value of the texture element that is nearest(in Manhattan distance) to the specified texture coordinates.
		LINEAR = GL_LINEAR //Returns the weighted average of the texture elements that are closest to the specified texture coordinates.These can include items wrapped or repeated from other parts of a texture, depending on the values of GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T, and on the exact mapping.
	};

	enum class TextureSwizzle {
		RED = GL_RED, // the value for r will be taken from the first channel of the fetched texel
		GREEN = GL_GREEN, // the value for r will be taken from the second channel of the fetched texel
		BLUE = GL_BLUE, //  the value for r will be taken from the third channel of the fetched texel
		ALPHA = GL_ALPHA, // the value for r will be taken from the fourth channel of the fetched texel
		ZERO = GL_ZERO, // the value for r will be subtituted with 0.0
		ONE = GL_ONE // the value for r will be subtituted with 1.0
	};

	enum class TextureWrap {
		CLAMP_TO_EDGE = GL_CLAMP_TO_EDGE, // causes tex coordinates to be clamped to the range [12N,1−12N], where N is the size of the texture in the direction of clamping
		CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER, // evaluates s coordinates in a similar manner to GL_CLAMP_TO_EDGE. However, in cases where clamping would have occurred in GL_CLAMP_TO_EDGE mode, the fetched texel data is substituted with the values specified by GL_TEXTURE_BORDER_COLOR
		MIRRORED_REPEAT = GL_MIRRORED_REPEAT, //causes the integer part of the tex coordinate to be ignored; the GL uses only the fractional part, thereby creating a repeating pattern
		REPEAT = GL_REPEAT, //causes the s coordinate to be set to the fractional part of the texture coordinate if the integer part of s is even; if the integer part of s is odd, then the s texture coordinate is set to 1−frac(s), where frac(s) represents the fractional part of s
		MIRROR_CLAMP_TO_EDGE = GL_MIRROR_CLAMP_TO_EDGE //causes the s coordinate to be repeated as for GL_MIRRORED_REPEAT for one repetition of the texture, at which point the coordinate to be clamped as in GL_CLAMP_TO_EDGE
	};

	//https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexParameter.xhtml
	enum TextureIParameter : unsigned {
		DEPTH_STENCIL_TEXTURE_MODE = GL_DEPTH_STENCIL_TEXTURE_MODE, //Specifies the mode used to read from depth-stencil format textures. params must be one of enum DepthStencilTextureMode. The initial value is GL_DEPTH_COMPONENT.

		BASE_LEVEL = GL_TEXTURE_BASE_LEVEL, // Specifies the index of the lowest defined mipmap level. This is an integer value. The initial value is 0.

		COMPARE_FUNC = GL_TEXTURE_COMPARE_FUNC, //Specifies the comparison operator used when GL_TEXTURE_COMPARE_MODE is set to GL_COMPARE_REF_TO_TEXTURE, enum CompareFunc
		COMPARE_MODE = GL_TEXTURE_COMPARE_MODE, //Specifies the texture comparison mode for currently bound depth textures. That is, a texture whose internal format is GL_DEPTH_COMPONENT Permissible values are: enum TextureCompareMode

		MIN_FILTER = GL_TEXTURE_MIN_FILTER, //The initial value of GL_TEXTURE_MIN_FILTER is GL_NEAREST_MIPMAP_LINEAR. The texture minifying function is used whenever the level-of-detail function used when sampling from the texture determines that the texture should be minified. There are six defined minifying functions. Two of them use either the nearest texture elements or a weighted average of multiple texture elements to compute the texture value. The other four use mipmaps.
		MAG_FILTER = GL_TEXTURE_MAG_FILTER, //The initial value of GL_TEXTURE_MAG_FILTER is GL_LINEAR. The texture magnification function is used whenever the level-of-detail function used when sampling from the texture determines that the texture should be magified. It sets the texture magnification function to either GL_NEAREST or GL_LINEAR (see below). GL_NEAREST is generally faster than GL_LINEAR, but it can produce textured images with sharper edges because the transition between texture elements is not as smooth.

		MAX_LEVEL = GL_TEXTURE_MAX_LEVEL, //Sets the index of the highest defined mipmap level. This is an integer value. The initial value is 1000.

		SWIZZLE_R = GL_TEXTURE_SWIZZLE_R, //Sets the swizzle that will be applied to the r component of a texel before it is returned to the shader. The initial value is GL_RED.
		SWIZZLE_G = GL_TEXTURE_SWIZZLE_G, //The initial value is GL_GREEN.
		SWIZZLE_B = GL_TEXTURE_SWIZZLE_B, //The initial value is GL_BLUE.
		SWIZZLE_A = GL_TEXTURE_SWIZZLE_A, //The initial value is GL_ALPHA.

		WRAP_S = GL_TEXTURE_WRAP_S, //Initially, is set to GL_REPEAT
		WRAP_T = GL_TEXTURE_WRAP_T, //Initially, is set to GL_REPEAT
		WRAP_R = GL_TEXTURE_WRAP_R //Initially, is set to GL_REPEAT
	};

	struct Swizzle {
		TextureSwizzle R = TextureSwizzle::RED;
		TextureSwizzle G = TextureSwizzle::GREEN;
		TextureSwizzle B = TextureSwizzle::BLUE;
		TextureSwizzle A = TextureSwizzle::ALPHA;
	};

	struct Wrap {
		TextureWrap S = TextureWrap::REPEAT;
		TextureWrap T = TextureWrap::REPEAT;
		TextureWrap R = TextureWrap::REPEAT;
	};

	enum TextureFParameter : unsigned {
		MIN_LOD = GL_TEXTURE_MIN_LOD, //Sets the minimum level-of-detail parameter. This floating-point value limits the selection of highest resolution mipmap (lowest mipmap level). The initial value is -1000.
		MAX_LOD = GL_TEXTURE_MAX_LOD, //Sets the maximum level-of-detail parameter. This floating-point value limits the selection of the lowest resolution mipmap (highest mipmap level). The initial value is 1000.
		LOD_BIAS = GL_TEXTURE_LOD_BIAS, //params specifies a fixed bias value that is to be added to the level-of-detail parameter for the texture before texture sampling. The specified value is added to the shader-supplied bias value (if any) and subsequently clamped into the implementation-defined range [−biasmax,biasmax], where biasmax is the value of the implementation defined constant GL_MAX_TEXTURE_LOD_BIAS. The initial value is 0.0.
	};

	enum TextureType : unsigned {
		TEXTURE_1D = GL_TEXTURE_1D,
		TEXTURE_2D = GL_TEXTURE_2D,
		TEXTURE_3D = GL_TEXTURE_3D,
		TEXTURE_RECTANGLE = GL_TEXTURE_RECTANGLE,
		//TEXTURE_BUFFER = GL_TEXTURE_BUFFER,
		TEXTURE_CUBE_MAP = GL_TEXTURE_CUBE_MAP,
		TEXTURE_1D_ARRAY = GL_TEXTURE_1D_ARRAY,
		TEXTURE_2D_ARRAY = GL_TEXTURE_2D_ARRAY,
		TEXTURE_CUBE_MAP_ARRAY = GL_TEXTURE_CUBE_MAP_ARRAY,
		TEXTURE_2D_MULTISAMPLE = GL_TEXTURE_2D_MULTISAMPLE,
		TEXTURE_2D_MULTISAMPLE_ARRAY = GL_TEXTURE_2D_MULTISAMPLE_ARRAY
	};

	enum PixelStorageMode {
		PACK_ALIGNMENT = GL_PACK_ALIGNMENT,
		// Specifies the alignment requirements for the start of each pixel row in memory.The allowable values are 1 (byte - alignment), 2 (rows aligned to even - numbered bytes), 4 (word - alignment), and 8 (rows start on double - word boundaries).

		UNPACK_ALIGNMENT = GL_UNPACK_ALIGNMENT,
		// Specifies the alignment requirements for the start of each pixel row in memory.The allowable values are 1 (byte - alignment), 2 (rows aligned to even - numbered bytes), 4 (word - alignment), and 8 (rows start on double - word boundaries).
	};

	enum class PixelStorageModeValue : int {
		BYTE = 1,
		NUMBERED_BYTE = 2,
		WORD = 4,
		WORD_BOUNDARIES = 8,
	};

	enum class CubeMapFaces {
		POSITIVE_X = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		NEGATIVE_X = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		POSITIVE_Y = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		NEGATIVE_Y = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		POSITIVE_Z = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		NEGATIVE_Z = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
	};
	struct Texture;

	constexpr inline void bindTexture(TextureType type, unsigned id) {
		glBindTexture(type, id);
	}

	constexpr inline void setActiveTextureSlot(unsigned slot) {
		glActiveTexture(TextureSlot(slot));
	}

	constexpr inline void bindTextureToSlot(unsigned slot, TextureType type, unsigned id) {
		setActiveTextureSlot(slot);
		bindTexture(type, id);
	}

	constexpr inline void bindTextureToSlot(unsigned slot, Texture* texture);
	constexpr inline void bindTexture(Texture* texture);

	struct Texture {
		Texture(const Texture& other) = delete;
		Texture(Texture&& other) noexcept = delete;
		Texture& operator=(const Texture& other) = delete;
		Texture& operator=(Texture&& other) noexcept = delete;
	public:

		

		struct Parameters {
			DepthStencilTextureMode depthStencilTextureMode = DepthStencilTextureMode::DEPTH_COMPONENT;
			int baseLevel = 0;

			CompareFunc compareFunc = CompareFunc::NEVER;
			TextureCompareMode compareMode = TextureCompareMode::NONE;

			TextureMinFilter minFilter = TextureMinFilter::NEAREST_MIPMAP_LINEAR;
			TextureMagFilter magFilter = TextureMagFilter::LINEAR;

			int maxLevel = 1000;

			Swizzle swizzle;
			Wrap wrap;

			void apply(Texture* texture) {
				texture->setParameter({
					{DEPTH_STENCIL_TEXTURE_MODE, static_cast<int>(depthStencilTextureMode)},

					{BASE_LEVEL, baseLevel},

					{COMPARE_FUNC, static_cast<int>(compareFunc)},
					{COMPARE_MODE, static_cast<int>(compareMode)},

					{MIN_FILTER, static_cast<int>(minFilter)},
					{MAG_FILTER, static_cast<int>(magFilter)},

					{MAX_LEVEL, maxLevel},

					{SWIZZLE_R, static_cast<int>(swizzle.R)},
					{SWIZZLE_G, static_cast<int>(swizzle.G)},
					{SWIZZLE_B, static_cast<int>(swizzle.B)},
					{SWIZZLE_A, static_cast<int>(swizzle.A)},

					{WRAP_R, static_cast<int>(wrap.R)},
					{WRAP_S, static_cast<int>(wrap.S)},
					{WRAP_T, static_cast<int>(wrap.T)},
					}
				);
			}
		};

		Parameters parameters;

		PixelFormat pixelFormat = RGBA8;
		TextureFormat textureFormat = RGBA;
		PixelDataType pixelType = UNSIGNED_BYTE;

		PixelStorageModeValue packAlignmentMode = PixelStorageModeValue::WORD;
		PixelStorageModeValue unpackAlignmentMode = PixelStorageModeValue::WORD;

		void applyPixelStorageMode() {
			glPixelStorei(PACK_ALIGNMENT, static_cast<int>(packAlignmentMode));
			glPixelStorei(UNPACK_ALIGNMENT, static_cast<int>(unpackAlignmentMode));
		}

		TextureType mType = TEXTURE_2D;

		int width = 0;
		int height = 0;

		int depth = 0;

	public:
		Texture() = default;
		Texture(TextureType type) : mType(type) {
			glGenTextures(1, &mId);//todo all gl commands should be called in gl context thread, need to find some way to do it always for any objects
		}

		~Texture() {
			glDeleteTextures(1, &mId);
		}

		void create(const void* data = nullptr) {
			if (!glIsTexture(mId)) {
				generate();
			}

			if (mType == TEXTURE_2D) {
				bindTextureToSlot(0, mType, mId);

				image2D(width, height, pixelFormat, textureFormat, pixelType, data);

				parameters.apply(this);
				applyPixelStorageMode();

				bindTexture(mType, 0);
			}
			else if (mType == TEXTURE_3D) {
				bindTextureToSlot(0, mType, mId);

				image3D(width, height, depth, pixelFormat, textureFormat, pixelType, data);

				parameters.apply(this);
				applyPixelStorageMode();

				bindTexture(mType, 0);
			}
		}

		void create2D(int width, int height, PixelFormat pixelFormat = RGBA8, TextureFormat format = RGBA, std::initializer_list<std::pair<SFE::GLW::TextureIParameter, unsigned>> parametersI = {}, std::initializer_list<std::pair<SFE::GLW::TextureFParameter, float>> parametersF = {}, PixelDataType type = UNSIGNED_BYTE,  const void* data = nullptr) {
			if (!glIsTexture(mId)) {
				generate();
			}

			bindTextureToSlot(0, mType, mId);

			image2D(width, height, pixelFormat, format, type, data);
			setParameter(parametersI);
			setParameter(parametersF);

			bindTexture(mType, 0);
		}

		void create3D(int width, int height, int depth, PixelFormat pixelFormat = RGBA8, TextureFormat format = RGBA, std::initializer_list<std::pair<SFE::GLW::TextureIParameter, unsigned>> parametersI = {}, std::initializer_list<std::pair<SFE::GLW::TextureFParameter, float>> parametersF = {}, PixelDataType type = UNSIGNED_BYTE, const void* data = nullptr) {
			if (!glIsTexture(mId)) {
				generate();
			}

			bindTextureToSlot(0, mType, mId);

			image3D(width, height, depth, pixelFormat, format, type, data);
			setParameter(parametersI);
			setParameter(parametersF);

			bindTexture(mType, 0);
		}

		void generate() {
			glGenTextures(1, &mId);
		}

		void image2D(int width, int height, PixelFormat pixelFormat = RGBA8, TextureFormat format = RGBA, PixelDataType type = UNSIGNED_BYTE, const void* data = nullptr) const {
			glTexImage2D(mType, /*lod-level*/0, pixelFormat, width, height, 0, format, type, data);
		}
		void image2D(int target, int width, int height, PixelFormat pixelFormat = RGBA8, TextureFormat format = RGBA, PixelDataType type = UNSIGNED_BYTE, const void* data = nullptr) const {
			glTexImage2D(target, /*lod-level*/0, pixelFormat, width, height, 0, format, type, data);
		}
		void image3D(int width, int height, int depth, PixelFormat pixelFormat = RGBA8, TextureFormat format = RGBA, PixelDataType type = UNSIGNED_BYTE, const void* data = nullptr) const {
			glTexImage3D(mType, /*lod-level*/0, pixelFormat, width, height, depth, 0, format, type, data);
		}

		void bind() const {
			bindTexture(mType, mId);
		}

		void bind(std::function<void(const Texture*)> func) const {
			bindTextureToSlot(0, mType, mId);
			func(this);
			bindTexture(mType, 0);
		}

		void setSubImageData2D(int xoffset, int yoffset, int w, int h, const void* data, TextureFormat format = RGBA, PixelDataType type = UNSIGNED_BYTE) const {
			glTexSubImage2D(mType, 0, xoffset, yoffset, w, h, format, type, data);
		}

		void setParameter(SFE::GLW::TextureIParameter param, unsigned value) const {
			glTexParameteri(mType, param, value);
		}
		void setParameter(std::initializer_list<std::pair<SFE::GLW::TextureIParameter, unsigned>> parameters) const {
			for (auto& [param, value] : parameters) {
				glTexParameteri(mType, param, value);
			}
		}

		void setParameter(SFE::GLW::TextureFParameter param, float value) const {
			glTexParameterf(mType, param, value);
		}

		void setParameter(std::initializer_list<std::pair<SFE::GLW::TextureFParameter, float>> parameters) const {
			for (auto& [param, value] : parameters) {
				glTexParameterf(mType, param, value);
			}
		}

		void setPixelStorageMode(SFE::GLW::PixelStorageMode mode, SFE::GLW::PixelStorageModeValue value) const {
			glPixelStorei(mode, static_cast<int>(value));
		}

		bool isValid() const {
			return this != nullptr && glIsTexture(mId);
		}

		unsigned mId = 0;
		
	};

	constexpr inline void bindTextureToSlot(unsigned slot, Texture* texture) {
		if (!texture) {
			return;
		}

		bindTextureToSlot(slot, texture->mType, texture->mId);
	}

	constexpr inline void bindTexture(Texture* texture) {
		bindTexture(texture->mType, texture->mId);
	}
}
