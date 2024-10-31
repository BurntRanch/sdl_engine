#include "ui.hpp"
#include "common.hpp"
#include "ui/arrows.hpp"
#include "ui/label.hpp"
#include "util.hpp"
#include <SDL3/SDL_stdinc.h>
#include <filesystem>
#include <freetype/freetype.h>
#include <glm/ext/matrix_transform.hpp>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>
#include <rapidxml.hpp>
#include "engine.hpp"

using namespace UI;

Panel::~Panel() {
    DestroyBuffers();
};

Panel::Panel(EngineSharedContext &sharedContext, glm::vec3 color, glm::vec2 position, glm::vec2 scales, float zDepth)
    : m_SharedContext(sharedContext) {

    type = PANEL;
        
    texture = CreateSinglePixelImage(sharedContext, color);
    
    SetDimensions(glm::vec4(position, scales));
    SetDepth(zDepth);
}

inline void Panel::SetPosition(glm::vec2 position) {
    m_Position = position;
    m_Dimensions.x = position.x;
    m_Dimensions.y = position.y;
}

inline void Panel::SetScales(glm::vec2 scales) {
    m_Dimensions.z = scales.x;
    m_Dimensions.w = scales.y;
}

inline void Panel::SetDimensions(glm::vec4 dimensions) {
    m_Dimensions = dimensions;
}

glm::vec4 Panel::GetDimensions() {
    return m_Dimensions;
}

void Panel::DestroyBuffers() {
    vkDestroyImage(m_SharedContext.engineDevice, texture.imageAndMemory.image, NULL);
    vkFreeMemory(m_SharedContext.engineDevice, texture.imageAndMemory.memory, NULL);
}

Label::~Label() {
    DestroyBuffers();
}

Label::Label(EngineSharedContext &sharedContext, std::string text, std::filesystem::path fontPath, glm::vec2 position, float zDepth)
    : m_SharedContext(sharedContext) {

    type = LABEL;
    
    SetPosition(position);
    SetDepth(m_Depth);

    if (FT_Init_FreeType(&m_FTLibrary)) {
        throw std::runtime_error("Failed to initialize FreeType!");
    }

    if (FT_New_Face(m_FTLibrary, fontPath.c_str(), 0, &m_FTFace)) {
        throw std::runtime_error(fmt::format("Failed to locate the requested font ({}) in your system!", fontPath.string()));
    }

    FT_Set_Pixel_Sizes(m_FTFace, 0, PIXEL_HEIGHT);

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
        Glyph glyph = m_SharedContext.engine->GenerateGlyph(m_SharedContext, m_FTFace, c, x, y, m_Depth);

        if (!glyph.glyphBuffer.has_value()) {
            continue;
        }

        Glyphs.push_back(glyph);
    }
}

std::optional<std::pair<TextureImageAndMemory, BufferAndMemory>> Label::GenerateGlyph(char c, float &x, float &y) {
    if (FT_Load_Char(m_FTFace, c, FT_LOAD_RENDER)) {
        throw std::runtime_error(fmt::format("Failed to load the glyph for '{}' with FreeType", c));
    }

    if (c == ' ') {
        x += m_FTFace->glyph->advance.x >> 6;

        return {};
    }

    if (c == '\n') {
        x = 0;
        y += PIXEL_HEIGHT;

        return {};
    }

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
    ypos -= 1.0f - (PIXEL_HEIGHT_FLOAT / static_cast<float>(m_SharedContext.settings.DisplayHeight));

    BufferAndMemory bufferAndMemory = CreateSimpleVertexBuffer(m_SharedContext, {
                                                            {glm::vec3(xpos, ypos, m_Depth), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec3(xpos + w, ypos + h, m_Depth), glm::vec2(1.0f, 1.0f)},
                                                            {glm::vec3(xpos, ypos + h, m_Depth), glm::vec2(0.0f, 1.0f)},
                                                            {glm::vec3(xpos, ypos, m_Depth), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec3(xpos + w, ypos, m_Depth), glm::vec2(1.0f, 0.0f)},
                                                            {glm::vec3(xpos + w, ypos + h, m_Depth), glm::vec2(1.0f, 1.0f)}
                                                        }, false);

    // The bitshift by 6 is required because Advance is 1/64th of a pixel.
    x += m_FTFace->glyph->advance.x >> 6;

    return std::make_pair(textureImageAndMemory, bufferAndMemory);
}

void Label::DestroyBuffers() {
    // vkDeviceWaitIdle(m_SharedContext.engineDevice);

    /* As of now, Glyph Buffers are now owned by the engine. */
    // for (size_t i = 0; i < Glyphs.size(); i++) {
    //     auto glyphBuffer = Glyphs[i].glyphBuffers.value();

    //     vkDestroyImage(m_SharedContext.engineDevice, glyphBuffer.first.imageAndMemory.image, NULL);
    //     vkFreeMemory(m_SharedContext.engineDevice, glyphBuffer.first.imageAndMemory.memory, NULL);

    //     vkDestroyBuffer(m_SharedContext.engineDevice, glyphBuffer.second.buffer, NULL);
    //     vkFreeMemory(m_SharedContext.engineDevice, glyphBuffer.second.memory, NULL);
    // }

    Glyphs.clear();
}

Arrows::Arrows(Model &highlightedModel) {
    type = ARROWS;

    arrowsModel = new Model("models/arrows.obj");
    arrowsModel->SetParent(&highlightedModel);

    arrowsModel->SetScale(glm::vec3(0.5f, 0.5f, 0.5f));
}

inline glm::vec2 GenericElement::GetPosition() {
    return m_Position;
}

inline void GenericElement::SetPosition(glm::vec2 Position) {
    m_Position = Position;
}

inline float GenericElement::GetDepth() {
    return m_Depth;
}

inline void GenericElement::SetDepth(float depth) {
    m_Depth = depth;
}

inline void GenericElement::DestroyBuffers() {
    throw std::runtime_error("You're calling DestroyBuffers on a GenericElement, this is wrong.");
}

GenericElement *DeserializeUIElement(EngineSharedContext &sharedContext, rapidxml::xml_node<char> *node) {
    using namespace rapidxml;
    std::string nodeName = node->name();

    GenericElement *element;

    if (nodeName == "Panel") {
        xml_node<char> *colorNode = node->first_node("Color");
        NULLASSERT(colorNode);

        xml_node<char> *colorRNode = colorNode->first_node("R");
        NULLASSERT(colorRNode);
        float colorR = std::stof(colorRNode->value());

        xml_node<char> *colorGNode = colorNode->first_node("G");
        NULLASSERT(colorGNode);
        float colorG = std::stof(colorGNode->value());

        xml_node<char> *colorBNode = colorNode->first_node("B");
        NULLASSERT(colorBNode);
        float colorB = std::stof(colorBNode->value());


        xml_node<char> *positionNode = node->first_node("Position");
        NULLASSERT(positionNode);

        xml_node<char> *positionXNode = positionNode->first_node("X");
        NULLASSERT(positionXNode);
        float positionX = std::stof(positionXNode->value());

        xml_node<char> *positionYNode = positionNode->first_node("Y");
        NULLASSERT(positionYNode);
        float positionY = std::stof(positionYNode->value());


        xml_node<char> *scaleNode = node->first_node("Scale");
        NULLASSERT(scaleNode);

        xml_node<char> *scaleXNode = scaleNode->first_node("X");
        NULLASSERT(scaleXNode);
        float scaleX = std::stof(scaleXNode->value());

        xml_node<char> *scaleYNode = scaleNode->first_node("Y");
        NULLASSERT(scaleYNode);
        float scaleY = std::stof(scaleYNode->value());

        xml_node<char> *zDepthNode = node->first_node("ZDepth");
        float zDepth = 1.0f;

        if (zDepthNode) {
            zDepth = std::stof(zDepthNode->value());
        }

        element = new UI::Panel(sharedContext, glm::vec3(colorR, colorG, colorB), glm::vec2(positionX, positionY), glm::vec2(scaleX, scaleY), zDepth);
    } else if (nodeName == "Label") {
        xml_node<char> *textNode = node->first_node("Text");
        NULLASSERT(textNode);
        std::string text = textNode->value();

        xml_node<char> *positionNode = node->first_node("Position");
        NULLASSERT(positionNode);

        xml_node<char> *positionXNode = positionNode->first_node("X");
        NULLASSERT(positionXNode);
        float positionX = std::stof(positionXNode->value());

        xml_node<char> *positionYNode = positionNode->first_node("Y");
        NULLASSERT(positionYNode);
        float positionY = std::stof(positionYNode->value());


        xml_node<char> *fontNode = node->first_node("Font");
        NULLASSERT(fontNode);
        std::string fontPath = fontNode->value();

        xml_node<char> *zDepthNode = node->first_node("ZDepth");
        float zDepth = 1.0f;

        if (zDepthNode) {
            zDepth = std::stof(zDepthNode->value());
        }

        element = new UI::Label(sharedContext, text, fontPath, glm::vec2(positionX, positionY), zDepth);
    } else {
        throw std::runtime_error(fmt::format("Unknown UI Serialized Object Type: {}", nodeName));
    }

    return element;
}

std::vector<GenericElement *> UI::LoadUIFile(EngineSharedContext &sharedContext, std::string_view fileName) {
    using namespace rapidxml;

    std::ifstream fileStream(fileName.data(), std::ios::binary | std::ios::ate);

    if (!fileStream.good())
        return {};

    std::vector<char> uiSceneRawXML(static_cast<int>(fileStream.tellg()) + 1);
    
    fileStream.seekg(0);
    fileStream.read(uiSceneRawXML.data(), uiSceneRawXML.size());

    xml_document<char> uiSceneXML;

    uiSceneXML.parse<0>(uiSceneRawXML.data());

    std::vector<GenericElement *> elements;

    xml_node<char> *uiSceneNode = uiSceneXML.first_node("UIScene");
    for (xml_node<char> *uiElement = uiSceneNode->first_node(); uiElement; uiElement = uiElement->next_sibling()) {
        elements.push_back(DeserializeUIElement(sharedContext, uiElement));
    }

    return elements;
}
