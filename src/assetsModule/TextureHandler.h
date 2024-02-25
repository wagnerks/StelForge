#pragma once

#include <array>
#include <map>
#include <string>
#include <unordered_map>

#include "Asset.h"
#include "core/Singleton.h"
#include "glad/glad.h"

namespace AssetsModule {

	enum class eTextureChannels {
		
	};

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

	enum PixelFormat : GLenum {
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

	//https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexParameter.xhtml
	enum TextureIParameter : unsigned {
		DEPTH_STENCIL_TEXTURE_MODE = GL_DEPTH_STENCIL_TEXTURE_MODE,
		TEXTURE_BASE_LEVEL = GL_TEXTURE_BASE_LEVEL,
		TEXTURE_COMPARE_FUNC = GL_TEXTURE_COMPARE_FUNC, // GL_LEQUAL	result = { 1.00.0  r <= Dtr > Dt
														// GL_GEQUAL	result = {1.00.0  r >= Dtr < Dt
														// GL_LESS	result = {1.00.0  r < Dtr >= Dt
														// GL_GREATER	result = {1.00.0  r > Dtr <= Dt
														// GL_EQUAL	result = {1.00.0  r = Dtr≠Dt
														// GL_NOTEQUAL	result = {1.00.0  r≠Dtr = Dt
														// GL_ALWAYS	result = 1.0
														// GL_NEVER

		TEXTURE_COMPARE_MODE = GL_TEXTURE_COMPARE_MODE, //Specifies the texture comparison mode for currently bound depth textures. That is, a texture whose internal format is GL_DEPTH_COMPONENT Permissible values are:
															//GL_COMPARE_REF_TO_TEXTURE
															//Specifies that the interpolated and clamped r texture coordinate should be compared to the value in the currently bound depth texture.
															//See the discussion of GL_TEXTURE_COMPARE_FUNC for details of how the comparison is evaluated.
															//The result of the comparison is assigned to the red channel.

															//GL_NONE
															//Specifies that the red channel should be assigned the appropriate value from the currently bound depth texture.

		TEXTURE_LOD_BIAS = GL_TEXTURE_LOD_BIAS,
		TEXTURE_MIN_FILTER = GL_TEXTURE_MIN_FILTER,
		TEXTURE_MAG_FILTER = GL_TEXTURE_MAG_FILTER,
		TEXTURE_MAX_LEVEL = GL_TEXTURE_MAX_LEVEL,
		TEXTURE_SWIZZLE_R = GL_TEXTURE_SWIZZLE_R,
		TEXTURE_SWIZZLE_G = GL_TEXTURE_SWIZZLE_G,
		TEXTURE_SWIZZLE_B = GL_TEXTURE_SWIZZLE_B,
		TEXTURE_SWIZZLE_A = GL_TEXTURE_SWIZZLE_A,
		TEXTURE_WRAP_S = GL_TEXTURE_WRAP_S,
		TEXTURE_WRAP_T = GL_TEXTURE_WRAP_T,
		TEXTURE_WRAP_R = GL_TEXTURE_WRAP_R
	};

	enum TextureFParameter : unsigned {
		TEXTURE_MIN_LOD = GL_TEXTURE_MIN_LOD, //Sets the minimum level-of-detail parameter. This floating-point value limits the selection of highest resolution mipmap (lowest mipmap level). The initial value is -1000.
		TEXTURE_MAX_LOD = GL_TEXTURE_MAX_LOD, //Sets the maximum level-of-detail parameter. This floating-point value limits the selection of the lowest resolution mipmap (highest mipmap level). The initial value is 1000.
	};

	enum TextureType : unsigned {
		TEXTURE_1D = GL_TEXTURE_1D,
		TEXTURE_2D = GL_TEXTURE_2D,
		TEXTURE_3D = GL_TEXTURE_3D,
		TEXTURE_RECTANGLE = GL_TEXTURE_RECTANGLE,
		TEXTURE_BUFFER = GL_TEXTURE_BUFFER,
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

	class Texture : public Asset {
	public:
		Texture(const Texture& other) = delete;
		Texture(Texture&& other) noexcept = delete;
		Texture& operator=(const Texture& other) = delete;
		Texture& operator=(Texture&& other) noexcept = delete;

		Texture() = default;
		Texture(unsigned id, TextureType type) : mId(id), mType(type) {}
		Texture(TextureType type) : mType(type) { glGenTextures(1, &mId); }//todo all gl commands should be called in gl context thread, need to find some way to do it always for any objects
		~Texture() override { glDeleteTextures(1, &mId); }

		void image2D(int width, int height, PixelFormat pixelFormat, TextureFormat format, PixelDataType type, const void* data = nullptr);
		void image3D(int width, int height, int depth, PixelFormat pixelFormat, TextureFormat format, PixelDataType type, const void* data = nullptr);
		void bind() const;
		void setSubImageData2D(int xoffset, int yoffset, int w, int h, const void* data, TextureFormat format, PixelDataType type, bool bind = true);

		void setParameter(TextureIParameter param, unsigned value);
		void setParameter(std::initializer_list<std::pair<TextureIParameter, unsigned>> parameters);

		void setParameter(TextureFParameter param, float value);
		void setParameter(std::initializer_list<std::pair<TextureFParameter, float>> parameters);
		void setPixelStorageMode(PixelStorageMode mode, PixelStorageModeValue value);
		unsigned mId = std::numeric_limits<unsigned>::max();
		TextureType mType = TextureType::TEXTURE_2D;

		bool isValid() const;
	};

	class TextureHandler : public SFE::Singleton<TextureHandler> {
		friend Singleton;
	public:
		void bindTextureToSlot(unsigned slot, TextureType type, unsigned int id);
		void bindTextureToSlot(unsigned slot, Texture* texture);
		static void setActiveTextureSlot(unsigned slot);
		unsigned getCurrentTexture(unsigned slot) const { return mBindedTextures[slot].first; }
		TextureType getCurrentTextureType(unsigned slot) const { return mBindedTextures[slot].second; }
		Texture mDefaultTex;

		Texture* loadTexture(const std::string& path, bool flip = false, PixelFormat pixelFormat = RGBA8, TextureFormat textureFormat = RGBA, PixelDataType pixelType = UNSIGNED_BYTE);
		Texture* loadCubemapTexture(const std::string& path, bool flip = false);
	private:
		std::array<std::pair<unsigned, TextureType>, 32> mBindedTextures{std::pair{std::numeric_limits<unsigned>::max(), TEXTURE_2D}};

	};
}
