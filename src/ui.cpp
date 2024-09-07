#include "ui.hpp"
#include "common.hpp"
#include "ui/label.hpp"
#include <SDL3/SDL_stdinc.h>
#include <filesystem>
#include <freetype/freetype.h>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

using namespace UI;

Panel::~Panel() {
    DestroyBuffers();
};

Panel::Panel(EngineSharedContext &sharedContext, glm::vec3 color, glm::vec4 dimensions) : m_SharedContext(sharedContext) {
    texture = CreateSinglePixelImage(sharedContext, color);

    vertex2DBuffer = CreateVertex2DBuffer(sharedContext, {
                                                            {glm::vec2(dimensions.x, dimensions.y), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec2(dimensions.x + dimensions.z, dimensions.y + dimensions.w), glm::vec2(1.0f, 1.0f)},
                                                            {glm::vec2(dimensions.x, dimensions.y + dimensions.w), glm::vec2(0.0f, 1.0f)},
                                                            {glm::vec2(dimensions.x, dimensions.y), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec2(dimensions.x + dimensions.z, dimensions.y), glm::vec2(1.0f, 0.0f)},
                                                            {glm::vec2(dimensions.x + dimensions.z, dimensions.y + dimensions.w), glm::vec2(1.0f, 1.0f)}
                                                        });
}

void Panel::DestroyBuffers() {
    vkDeviceWaitIdle(m_SharedContext.engineDevice);

    vkDestroyBuffer(m_SharedContext.engineDevice, vertex2DBuffer.buffer, NULL);
    vkFreeMemory(m_SharedContext.engineDevice, vertex2DBuffer.memory, NULL);

    vkDestroyImage(m_SharedContext.engineDevice, texture.imageAndMemory.image, NULL);
    vkFreeMemory(m_SharedContext.engineDevice, texture.imageAndMemory.memory, NULL);
}




Label::~Label() {
    DestroyBuffers();
}

Label::Label(EngineSharedContext &sharedContext, std::string text, std::filesystem::path fontPath, glm::vec2 position) : Position(position), m_SharedContext(sharedContext) {
    if (FT_Init_FreeType(&m_FTLibrary)) {
        throw std::runtime_error("Failed to initialize FreeType!");
    }

    if (FT_New_Face(m_FTLibrary, fontPath.c_str(), 0, &m_FTFace)) {
        throw std::runtime_error(fmt::format("Failed to locate the requested font ({}) in your system!", fontPath.string()));
    }

    FT_Set_Pixel_Sizes(m_FTFace, 0, 64);

    float x = 0.5f;

    /* This value needs to be -1.5 for SOME reason. 
     *
     *  If it is too high, the glyph shifts to the bottom and artifacts start showing from above.
     *  If it is too low, the glyph shifts to the top and artifacts start showing from below.
     *
     * Tested against NotoSans-Black.ttf and LiberationMono-Bold.ttf
     */
    float y = -1.5f;

    for (char c : text) {
        auto glyph = GenerateGlyph(c, x, y);

        if (!glyph.has_value()) {
            continue;
        }

        GlyphBuffers.push_back(std::make_pair(c, glyph.value()));
    }
}

std::optional<std::pair<TextureImageAndMemory, BufferAndMemory>> Label::GenerateGlyph(char c, float &x, float &y) {
    if (c == ' ') {
        x += m_FTFace->glyph->advance.x >> 6;

        return {};
    }

    if (FT_Load_Char(m_FTFace, c, FT_LOAD_RENDER)) {
        throw std::runtime_error(fmt::format("Failed to load the glyph for '{}' with FreeType", c));
    }

    m_StringWidth += ((m_FTFace->glyph->advance.x >> 6)/1920.0f);

    VkDeviceSize glyphBufferSize = m_FTFace->glyph->bitmap.width * m_FTFace->glyph->bitmap.rows;

    TextureBufferAndMemory glyphBuffer{};
    AllocateBuffer(m_SharedContext, glyphBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyphBuffer.bufferAndMemory.buffer, glyphBuffer.bufferAndMemory.memory);
    glyphBuffer.width = m_FTFace->glyph->bitmap.width;
    glyphBuffer.height = m_FTFace->glyph->bitmap.rows;
    glyphBuffer.channels = 1;

    vkMapMemory(m_SharedContext.engineDevice, glyphBuffer.bufferAndMemory.memory, 0, glyphBufferSize, 0, &(glyphBuffer.bufferAndMemory.mappedData));
    SDL_memcpy(glyphBuffer.bufferAndMemory.mappedData, m_FTFace->glyph->bitmap.buffer, glyphBufferSize);

    TextureImageAndMemory textureImageAndMemory = CreateImage(m_SharedContext, m_FTFace->glyph->bitmap.width, m_FTFace->glyph->bitmap.rows, VK_FORMAT_R8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ChangeImageLayout(m_SharedContext, textureImageAndMemory.imageAndMemory.image, 
                VK_FORMAT_R8_SRGB, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
    CopyBufferToImage(m_SharedContext, glyphBuffer, textureImageAndMemory.imageAndMemory.image);
    ChangeImageLayout(m_SharedContext, textureImageAndMemory.imageAndMemory.image, 
                VK_FORMAT_R8_SRGB, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

    vkDestroyBuffer(m_SharedContext.engineDevice, glyphBuffer.bufferAndMemory.buffer, NULL);
    vkFreeMemory(m_SharedContext.engineDevice, glyphBuffer.bufferAndMemory.memory, NULL);

    float xpos = (x + m_FTFace->glyph->bitmap_left)/static_cast<float>(m_SharedContext.settings.DisplayWidth);
    float ypos = (y - m_FTFace->glyph->bitmap_top)/static_cast<float>(m_SharedContext.settings.DisplayHeight);

    float w = (m_FTFace->glyph->bitmap.width)/static_cast<float>(m_SharedContext.settings.DisplayWidth);
    float h = (m_FTFace->glyph->bitmap.rows)/static_cast<float>(m_SharedContext.settings.DisplayHeight);

    xpos -= 1.0f;
    ypos -= 1.0f - (64.0f / static_cast<float>(m_SharedContext.settings.DisplayHeight));

    BufferAndMemory bufferAndMemory = CreateVertex2DBuffer(m_SharedContext, {
                                                            {glm::vec2(xpos, ypos), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec2(xpos + w, ypos + h), glm::vec2(1.0f, 1.0f)},
                                                            {glm::vec2(xpos, ypos + h), glm::vec2(0.0f, 1.0f)},
                                                            {glm::vec2(xpos, ypos), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec2(xpos + w, ypos), glm::vec2(1.0f, 0.0f)},
                                                            {glm::vec2(xpos + w, ypos + h), glm::vec2(1.0f, 1.0f)}
                                                        }, false);

    // The bitshift by 6 is required because Advance is 1/64th of a pixel.
    x += m_FTFace->glyph->advance.x >> 6;

    m_StringWidth += w;

    return std::make_pair(textureImageAndMemory, bufferAndMemory);
}

void Label::DestroyBuffers() {
    vkDeviceWaitIdle(m_SharedContext.engineDevice);

    for (size_t i = 0; i < GlyphBuffers.size(); i++) {
        auto glyphBuffer = GlyphBuffers[i];

        vkDestroyImage(m_SharedContext.engineDevice, glyphBuffer.second.first.imageAndMemory.image, NULL);
        vkFreeMemory(m_SharedContext.engineDevice, glyphBuffer.second.first.imageAndMemory.memory, NULL);

        vkDestroyBuffer(m_SharedContext.engineDevice, glyphBuffer.second.second.buffer, NULL);
        vkFreeMemory(m_SharedContext.engineDevice, glyphBuffer.second.second.memory, NULL);
    }

    GlyphBuffers.clear();
}
