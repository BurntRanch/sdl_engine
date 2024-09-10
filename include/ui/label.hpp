#pragma once
#include "common.hpp"
#include <ft2build.h>
#include <glm/ext/vector_float2.hpp>
#include <map>
#include FT_FREETYPE_H

#define PIXEL_HEIGHT 64
#define PIXEL_HEIGHT_FLOAT 64.0f

#define CALC_RELATIVE_PIXEL_HEIGHT(settings) (PIXEL_HEIGHT_FLOAT/static_cast<float>(settings.DisplayHeight))

namespace UI {
class Label {
public:
    // An array of textures for each character in use, not all ASCII characters.
    std::vector<std::pair<char, std::pair<TextureImageAndMemory, BufferAndMemory>>> GlyphBuffers;

    glm::vec2 Position;

    ~Label();

    Label(EngineSharedContext &sharedContext, std::string text, std::filesystem::path fontPath, glm::vec2 position = glm::vec2(0.0f, 0.0f));

    std::optional<std::pair<TextureImageAndMemory, BufferAndMemory>> GenerateGlyph(char c, float &x, float &y);

    void DestroyBuffers();
private:
    std::string m_Text;

    FT_Library m_FTLibrary;
    FT_Face m_FTFace;

    EngineSharedContext m_SharedContext;
};
}