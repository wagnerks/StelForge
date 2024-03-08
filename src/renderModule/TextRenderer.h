#pragma once

#include <map>
#include <string>

#include "Renderer.h"
#include "assetsModule/shaderModule/Shader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/FileSystem.h"
#include "logsModule/logger.h"
#include "mathModule/Forward.h"
#include "mathModule/Utils.h"

#include <ft2build.h>

#include "assetsModule/TextureHandler.h"
#include "glWrapper/BlendStack.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Draw.h"
#include "glWrapper/VertexArray.h"

#include FT_FREETYPE_H

namespace SFE::Render {
    struct GlyphInfo {
        Math::IVec2   size;      // Size of glyph
        Math::IVec2   bearing;   // Offset from baseline to left/top of glyph
        unsigned int advance;   // Horizontal offset to advance to next glyph
        std::pair<Math::Vec2, Math::Vec2>   texCoords; //min max glyphCoords in atlas
    };

    struct Font {
    	inline static unsigned int atlasSize = 512;
        Math::Vec2 glyphSize; //max glyph W and H

        Font(std::string_view path, uint16_t fontSize = 16) {
            load(path, fontSize);
        }

        void clear() const {
            if (mAtlasTex) {
                delete mAtlasTex;
            }
    	}

        const GlyphInfo& getGlyphData(char c) const {
            if (c >= mGlyphs.size()) {
                assert(false && "try to draw symbol which is not exists in font");
                return mGlyphs['?'];
            }
            return mGlyphs[c];
        }

        static Font loadFont(std::string_view path, uint16_t fontSize = 16) {
            return Font(path, fontSize);
        }

        void load(std::string_view path, uint16_t fontSize = 16) {
            FT_Library ft;
            if (FT_Init_FreeType(&ft)) {
                LogsModule::Logger::LOG_FATAL(false, "FREETYPE: Could not init FreeType Library");
                return;
            }

            FT_Face face;
            if (FT_New_Face(ft, path.data(), 0, &face)) {
                LogsModule::Logger::LOG_FATAL(false, "FREETYPE: Failed to load font %s", path.data());
                return;
            }

            if (!mFontPath.empty()) {
                delete mAtlasTex;
                mAtlasTex = nullptr;
                mGlyphs.clear();
            }

        	mFontPath = path;
            mFontSize = fontSize;

            FT_Set_Pixel_Sizes(face, 0, mFontSize);
            mAtlasHeight = calcAtlasHeight(face);
            mAtlasTex = new GLW::Texture{ GLW::TEXTURE_2D };
      
            mAtlasTex->width = atlasSize;
            mAtlasTex->height = mAtlasHeight;
            mAtlasTex->parameters.minFilter = GLW::TextureMinFilter::LINEAR;
            mAtlasTex->parameters.magFilter = GLW::TextureMagFilter::LINEAR;
            mAtlasTex->parameters.wrap.S = GLW::TextureWrap::CLAMP_TO_EDGE;
        	mAtlasTex->parameters.wrap.T = GLW::TextureWrap::CLAMP_TO_EDGE;
            mAtlasTex->pixelFormat = GLW::R8;
            mAtlasTex->textureFormat = GLW::RED;
            mAtlasTex->pixelType = GLW::UNSIGNED_BYTE;
            mAtlasTex->unpackAlignmentMode = GLW::PixelStorageModeValue::BYTE;
            mAtlasTex->create();
                        
            loadGlyphsToAtlas(face);

            FT_Done_Face(face);
            FT_Done_FreeType(ft);
        }

        unsigned int getAtlas() const { return mAtlasTex ? mAtlasTex->mId : 0; }
        GLW::Texture* getAtlasTex() const { return mAtlasTex; }

    private:
        std::string mFontPath;

        GLW::Texture* mAtlasTex = nullptr; //todo leak
        uint16_t mFontSize = 0;

        unsigned int mAtlasHeight = 0;

        std::vector<GlyphInfo> mGlyphs;

	    static int calcAtlasHeight(FT_Face face) {
            static int offset = 15;
            int x = 2, y = 2;//offsets for smooth glyphs

            FT_UInt index = 0;
            FT_ULong c = FT_Get_First_Char(face, &index);
            unsigned int rows = 0;

            while (index) {
                if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                    continue;
                }

                if (x + face->glyph->bitmap.width + offset >= atlasSize) {
                    x = 0;
                    y += face->glyph->bitmap.rows + offset;
                    rows = 0;
                }

                rows = std::max(face->glyph->bitmap.rows + offset, rows);

                x += face->glyph->bitmap.width + offset;

                c = FT_Get_Next_Char(face, c, &index);
            }

            return y + rows;
        }

        void loadGlyphsToAtlas(FT_Face face) {
            static int offset = 15;

            int x = 2, y = 2;

            FT_UInt index = 0;
            FT_ULong c = FT_Get_First_Char(face, &index);

            unsigned int rows = 0;
            mAtlasTex->bind();
            while (index) {
                if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                    continue;
                }

                if (x + face->glyph->bitmap.width + offset >= atlasSize) {
                    x = 2;
                    y += rows;
                    rows = 0;
                }
                rows = std::max(face->glyph->bitmap.rows + offset, rows);

                glyphSize.x = std::max(static_cast<float>(face->glyph->bitmap.width), glyphSize.x);
                glyphSize.y = std::max(static_cast<float>(face->glyph->bitmap.rows), glyphSize.y);

                mAtlasTex->setSubImageData2D(x, y, face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap.buffer, GLW::RED);

                if (mGlyphs.size() <= c) {
                    mGlyphs.resize(c + 1);
                }

                mGlyphs[c] = GlyphInfo{
                    {face->glyph->bitmap.width, face->glyph->bitmap.rows},
                    {face->glyph->bitmap_left, face->glyph->bitmap_top},
                    static_cast<unsigned int>(face->glyph->advance.x),
                    {
                        Math::Vec2{static_cast<float>(x) / atlasSize,static_cast<float>(y) / mAtlasHeight},
                        Math::Vec2{static_cast<float>(x + face->glyph->bitmap.width) / atlasSize,static_cast<float>(y + face->glyph->bitmap.rows) / mAtlasHeight}
                    }
                };

                x += face->glyph->bitmap.width + offset;

                c = FT_Get_Next_Char(face, c, &index);
            }
            mAtlasTex->unbind();
        }
    };

    class FontsRegistry : public Singleton<FontsRegistry> {
    public:
        Font* getFont(std::string_view path, uint16_t fontSize) {
            const auto it = mFonts.find({ std::string(path), fontSize });
	        if (it != mFonts.end()) {
                return &it->second;
	        }

            return &mFonts.insert({ {std::string(path), fontSize} , Font::loadFont(path, fontSize) }).first->second;
        }

        void deleteFont(std::string_view path, uint16_t fontSize) {
            const auto it = mFonts.find({ std::string(path), fontSize });
            if (it != mFonts.end()) {
                it->second.clear();
                mFonts.erase(it);
            }
        }

    protected:
        ~FontsRegistry() override {
            for (auto& [key, font] : mFonts) {
                font.clear();
            }

            mFonts.clear();
        }

    private:
        struct FontKey {
	        friend bool operator==(const FontKey& lhs, const FontKey& rhs) {
		        return lhs.path == rhs.path && lhs.fontSize == rhs.fontSize;
	        }

	        friend bool operator!=(const FontKey& lhs, const FontKey& rhs) {
		        return !(lhs == rhs);
	        }

	        std::string path;
            uint16_t fontSize;
        };


        struct KeyHasher {
            std::size_t operator()(const FontKey& s) const {
	            constexpr std::hash<std::string> hasher;
                return hasher(s.path + std::to_string(s.fontSize));
            }
        };

        std::unordered_map<FontKey, Font, KeyHasher> mFonts;
    };

	class TextRenderer : public Singleton<TextRenderer> {
	public:
        template <typename... Args>
        void renderText(const Math::Vec2& pos, const Math::Vec2& pivot, float scale, Math::Vec3 color, Font* font, const char* msg, const Args&... args) {
            constexpr size_t count = sizeof...(args);
            if (count > 0) {
                msg = formatInternal(msg, args...);
            }
            auto size = calcTextSize(font, scale, msg);

            renderText(std::string(msg), pos.x + -pivot.x * size.x, pos.y + (1.f-pivot.y) * size.y, scale, color, font);
        }
        template <typename... Args>
        Math::Vec2 calcTextSize(Font* font, float scale, const char* msg, const Args&... args) {
            constexpr size_t count = sizeof...(args);
            if (count > 0) {
                msg = formatInternal(msg, args...);
            }

            return calcTextSize(std::string(msg), font, scale);
        }

        Math::Vec2 calcTextSize(std::string text, Font* font, float scale) const {
            float rowH = 0.f;
            float rowW = 0.f;

            Math::Vec2 res;
            for (int i = 0; i < text.size(); i++) {
                const auto& ch = font->getGlyphData(text[i]);
                if (text[i] == '\n') {
                    res.y += rowH;
                    res.x = std::max(res.x, rowW);;
                    rowH = 0.f;
                    rowW = 0.f;
                }
                const float w = ch.size.x * scale;
                const float h = ch.size.y * scale;
                rowH = std::max(h, rowH);
                rowW += (ch.advance >> 6) * scale;
            }

            res.y += rowH;
            res.x = std::max(res.x, rowW);;

            return res;
        }

        void renderText(std::string text, float x, float y, float scale, Math::Vec3 color, Font* font) const {
            VBO.bind();
            VBO.allocateData(sizeof(float) * 6 * 4, text.size(), GLW::DYNAMIC_DRAW, nullptr);

            const float semiW = Render::Renderer::screenDrawData.width * 0.5f;
            const float semiH = Render::Renderer::screenDrawData.height * 0.5f;

            y = Render::Renderer::screenDrawData.height - y;

            int rowH = 0;
            float curX = x;
            for (int i = 0; i < text.size(); i++) {
                const auto& ch = font->getGlyphData(text[i]);
                if (text[i] == '\n') {
                    y -= font->glyphSize.y;
                    curX = x;
                    rowH = 0;
                }
                const float xpos = curX + ch.bearing.x * scale;
                const float ypos = y - (ch.size.y - ch.bearing.y) * scale;

                const float w = ch.size.x * scale;
                const float h = ch.size.y * scale;
                rowH = std::max(static_cast<int>(h), rowH);

                float startX = (xpos - semiW) / semiW;
                float startY = (ypos - semiH) / semiH;

                float endX = (xpos + w - semiW) / semiW;
                float endY = (ypos + h - semiH) / semiH;

                const float vertices[24] = {
                     startX,     endY,        ch.texCoords.first.x, ch.texCoords.first.y,
                     startX,     startY,      ch.texCoords.first.x, ch.texCoords.second.y,
                     endX,       startY,      ch.texCoords.second.x, ch.texCoords.second.y,
                     

                     startX,    endY,       ch.texCoords.first.x, ch.texCoords.first.y,
                     endX,      startY,     ch.texCoords.second.x, ch.texCoords.second.y,
                     endX,      endY,       ch.texCoords.second.x, ch.texCoords.first.y,
                     
                };
                VBO.setData(sizeof(vertices), 1, vertices, i);

                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                curX += (ch.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels)) 
            }

            VBO.bindDefaultBuffer(VBO.getType());
           

            GLW::CapabilitiesStack<GLW::BLEND>::push(true);
            GLW::BlendFuncStack::push({ GLW::SRC_ALPHA, GLW::ONE_MINUS_SRC_ALPHA });
            const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/text.vs", "shaders/text.fs");
            shader->use();
            shader->setUniform("textColor", color);
            shader->setUniform("text", 24);

            GLW::bindTextureToSlot(24, font->getAtlasTex());

            drawVertices(GLW::TRIANGLES, VAO.getID(), static_cast<int>(text.size() + 1) * 6, 0, 0);

            GLW::CapabilitiesStack<GLW::BLEND>::pop();
            GLW::BlendFuncStack::pop();
        }

        void init() override {
            const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/text.vs", "shaders/text.fs");
            shader->use();
            shader->setUniform("projection", Math::orthoRH_NO(0.0f, static_cast<float>(Renderer::screenDrawData.width), 0.f, static_cast<float>(Renderer::screenDrawData.height), -1.f, 1.f));

            VAO.generate();
            VAO.bind();
            VBO.bind();

            VAO.addAttribute(0, 4, GLW::AttributeFType::FLOAT, false, 4 * sizeof(float));
            VBO.bindDefaultBuffer(VBO.getType());
            VAO.bindDefault();
        }
        inline static char msgBuf[2048];
        template <typename... Args>
        static const char* formatInternal(const char* msg, const Args&... args) {
            snprintf(msgBuf, 2048, msg, args...);
            return msgBuf;
        }
	protected:
        ~TextRenderer() override {}

	private:

        GLW::VertexArray VAO;

        GLW::Buffer VBO{GLW::ARRAY_BUFFER };
	};
}
