#pragma once
#include "common.hpp"
#include <ft2build.h>
#include <glm/ext/vector_float2.hpp>
#include <map>
#include FT_FREETYPE_H

struct Character {
    glm::vec2 Size;
    glm::vec2 Bearing;
    Uint32 Advance;
};

namespace UI {
class Label {
public:
    // An array of textures for each character in use, not all ASCII characters.
    std::vector<std::pair<char, std::pair<TextureImageAndMemory, BufferAndMemory>>> glyphBuffers;
    std::map<char, Character> chars;

    ~Label();

    Label(EngineSharedContext &sharedContext, std::string text);

    std::pair<TextureImageAndMemory, BufferAndMemory> GenerateGlyph(char c, float &x, float &y);

    void DestroyBuffers();
private:
    std::string m_Text;

    FT_Library m_FTLibrary;
    FT_Face m_FTFace;

    EngineSharedContext m_SharedContext;
};
}