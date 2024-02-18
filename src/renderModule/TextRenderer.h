#pragma once

#include <map>
#include <string>

#include "Renderer.h"
#include "assetsModule/shaderModule/Shader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/FileSystem.h"
#include "glad/glad.h"
#include "logsModule/logger.h"
#include "mathModule/Forward.h"
#include "mathModule/Utils.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace SFE::RenderModule {
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
            if (!mFontPath.empty()) {
                glDeleteTextures(1, &mAtlas);
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
                glDeleteTextures(1, &mAtlas);
                mGlyphs.clear();
            }

        	mFontPath = path;
            mFontSize = fontSize;

            FT_Set_Pixel_Sizes(face, 0, mFontSize);
            mAtlasHeight = calcAtlasHeight(face);

            glGenTextures(1, &mAtlas);
            AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mAtlas);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasSize, mAtlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

            loadGlyphsToAtlas(face);

            FT_Done_Face(face);
            FT_Done_FreeType(ft);
        }

        unsigned int getAtlas() const { return mAtlas; }

    private:
        std::string mFontPath;

        unsigned int mAtlas = 0;
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

                glTexSubImage2D(GL_TEXTURE_2D, 0,
                    x, y,
                    face->glyph->bitmap.width, face->glyph->bitmap.rows,
                    GL_RED,
                    GL_UNSIGNED_BYTE,
                    face->glyph->bitmap.buffer
                );

                if (mGlyphs.size() <= c) {
                    mGlyphs.resize(c + 1);
                }

                mGlyphs[c] = GlyphInfo {
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
            const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/text.vs", "shaders/text.fs");
            shader->use();
            shader->setVec3("textColor", color);
            shader->setInt("text", 24);

            AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE24, GL_TEXTURE_2D, font->getAtlas());

            glBindVertexArray(VAO);
            glEnable(GL_BLEND);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * text.size(), nullptr, GL_DYNAMIC_DRAW);

            const float semiW = RenderModule::Renderer::SCR_WIDTH * 0.5f;
            const float semiH = RenderModule::Renderer::SCR_HEIGHT * 0.5f;

            y = RenderModule::Renderer::SCR_HEIGHT - y;

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

                glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices) * i, sizeof(vertices), vertices);

                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                curX += (ch.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels)) 
            }

            RenderModule::Renderer::drawArrays(GL_TRIANGLES, static_cast<int>(text.size() + 1) * 6);
            glDisable(GL_BLEND);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        void init() override {
            const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/text.vs", "shaders/text.fs");
            shader->use();
            shader->setMat4("projection", Math::orthoRH_NO(0.0f, static_cast<float>(Renderer::SCR_WIDTH), 0.f, static_cast<float>(Renderer::SCR_HEIGHT), -1.f, 1.f));

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        inline static char msgBuf[2048];
        template <typename... Args>
        static const char* formatInternal(const char* msg, const Args&... args) {
            snprintf(msgBuf, 2048, msg, args...);
            return msgBuf;
        }
	protected:
        ~TextRenderer() override {
            glDeleteBuffers(1, &VBO);
            glDeleteVertexArrays(1, &VAO);
        }

	private:

        

        unsigned int VAO = 0, VBO = 0;
	};
}
