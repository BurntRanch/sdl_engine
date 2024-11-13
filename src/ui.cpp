#include "ui.hpp"
#include "common.hpp"
#include "ui/arrows.hpp"
#include "ui/button.hpp"
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

    genericType = SCALABLE;
    type = PANEL;
        
    texture = CreateSinglePixelImage(sharedContext, color);
    
    SetPosition(position);
    SetScale(scales);
    SetDepth(zDepth);
}

inline void Panel::SetPosition(glm::vec2 position) {
    m_Position = position;
    m_Dimensions.x = position.x;
    m_Dimensions.y = position.y;
}

inline void Panel::SetScale(glm::vec2 scales) {
    m_Dimensions.z = scales.x;
    m_Dimensions.w = scales.y;
}

glm::vec4 Panel::GetDimensions() {
    if (m_Parent) {
        glm::vec2 parentPosition = m_Parent->GetPosition();
        glm::vec2 parentScale = (m_Parent->genericType == SCALABLE ? reinterpret_cast<Scalable *>(m_Parent)->GetUnfitScale() : glm::vec4(0.0f));

        glm::vec4 dimensions = m_Dimensions + glm::vec4(parentPosition, parentScale);
        glm::vec2 scales = adjustScaleToFitType(this, glm::vec2(dimensions.z, dimensions.w));

        dimensions.z = scales.x;
        dimensions.w = scales.y;

        return dimensions;
    }

    glm::vec2 scales = adjustScaleToFitType(this, glm::vec2(m_Dimensions.z, m_Dimensions.w));

    return glm::vec4(m_Dimensions.x, m_Dimensions.y, scales);
}

glm::vec2 Panel::GetPosition() {
    glm::vec4 dimensions = GetDimensions();

    return glm::vec2(dimensions.x, dimensions.y);
}

glm::vec2 Panel::GetScale() {
    glm::vec4 dimensions = GetDimensions();

    return glm::vec2(dimensions.z, dimensions.w);
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

    genericType = LABEL;
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

    float x = 0.0f;

    /* This value needs to be -1.5 for SOME reason. 
     *
     *  If it is too high, the glyph shifts to the bottom and artifacts start showing from above.
     *  If it is too low, the glyph shifts to the top and artifacts start showing from below.
     *
     * Tested against NotoSans-Black.ttf and LiberationMono-Bold.ttf
     */
    float y = 0.0f;

    for (char c : text) {
        Glyph glyph = m_SharedContext.engine->GenerateGlyph(m_SharedContext, m_FTFace, c, x, y, m_Depth);

        if (!glyph.glyphBuffer.has_value()) {
            continue;
        }

        Glyphs.push_back(glyph);
    }
}

glm::vec2 Label::CalculateMinimumScaleToFit() {
    glm::vec2 result = glm::vec2(0.0f, 0.0f);

    for (Glyph glyph : Glyphs) {
        result.x = std::max((glyph.offset.x + 1.0f)/2.0f + (glyph.scale.x)/2.0f, result.x);
        result.y = std::max((glyph.offset.y + 1.0f)/2.0f + (glyph.scale.y)/2.0f, result.y);
    }

    return result;
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
    genericType = ARROWS;
    type = ARROWS;

    arrowsModel = new Model("models/arrows.obj");
    arrowsModel->SetParent(&highlightedModel);

    arrowsModel->SetScale(glm::vec3(0.5f, 0.5f, 0.5f));
}

Button::Button(glm::vec2 position, glm::vec2 scale, Panel *panel, Label *label) : bgPanel(panel), fgLabel(label) {
    genericType = SCALABLE;
    type = BUTTON;

    SetPosition(position);
    SetScale(scale);

    bgPanel->SetParent(this);
    fgLabel->SetParent(panel); 
}

inline glm::vec2 GenericElement::GetPosition() {
    return m_Position + (m_Parent == nullptr ? glm::vec2(0.0f, 0.0f) : m_Parent->GetPosition());
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

inline void GenericElement::SetParent(GenericElement *parent) {
    m_Parent = parent;

    if (parent == nullptr) {
        return;
    }

    if (genericType == SCALABLE && parent->genericType == SCALABLE) {
        reinterpret_cast<Scalable *>(this)->fitType = reinterpret_cast<Scalable *>(this)->fitType;
    }

    parent->AddChild(this);
}

inline GenericElement *GenericElement::GetParent() {
    return m_Parent;
}

inline void GenericElement::AddChild(GenericElement *element) {
    m_Children.push_back(element);
}

inline void GenericElement::RemoveChild(GenericElement *child) {
    for (GenericElement *element : m_Children) {
        if (element != child) {
            continue;
        }

        m_Children.erase(std::find(m_Children.begin(), m_Children.end(), child));
    }
}

inline std::vector<GenericElement *> GenericElement::GetChildren() {
    return m_Children;
}

inline void GenericElement::DestroyBuffers() {
    throw std::runtime_error("You're calling DestroyBuffers on a GenericElement, this is wrong.");
}

inline void Scalable::SetScale(glm::vec2 scales) {
    m_Scale = scales;
}

inline glm::vec2 Scalable::GetScale() {
    glm::vec2 scale = m_Scale * (m_Parent != nullptr && m_Parent->genericType == SCALABLE ? reinterpret_cast<Scalable *>(m_Parent)->GetUnfitScale() : glm::vec3(1));

    scale = adjustScaleToFitType(this, scale);

    return scale;
}

inline glm::vec2 Scalable::GetUnfitScale() {
    glm::vec2 scale = m_Scale * (m_Parent != nullptr && m_Parent->genericType == SCALABLE ? reinterpret_cast<Scalable *>(m_Parent)->GetUnfitScale() : glm::vec3(1));

    return scale;
}

GenericElement *DeserializeUIElement(EngineSharedContext &sharedContext, rapidxml::xml_node<char> *node) {
    using namespace rapidxml;
    std::string nodeName = node->name();

    GenericElement *element;

    if (nodeName == "Panel") {
        xml_node<char> *fitTypeNode = node->first_node("FitType");
        
        xml_node<char> *colorNode = node->first_node("Color");
        UTILASSERT(colorNode);

        xml_node<char> *colorRNode = colorNode->first_node("R");
        UTILASSERT(colorRNode);
        float colorR = std::stof(colorRNode->value());

        xml_node<char> *colorGNode = colorNode->first_node("G");
        UTILASSERT(colorGNode);
        float colorG = std::stof(colorGNode->value());

        xml_node<char> *colorBNode = colorNode->first_node("B");
        UTILASSERT(colorBNode);
        float colorB = std::stof(colorBNode->value());


        xml_node<char> *positionNode = node->first_node("Position");
        UTILASSERT(positionNode);

        xml_node<char> *positionXNode = positionNode->first_node("X");
        UTILASSERT(positionXNode);
        float positionX = std::stof(positionXNode->value());

        xml_node<char> *positionYNode = positionNode->first_node("Y");
        UTILASSERT(positionYNode);
        float positionY = std::stof(positionYNode->value());


        xml_node<char> *scaleNode = node->first_node("Scale");
        UTILASSERT(scaleNode);

        xml_node<char> *scaleXNode = scaleNode->first_node("X");
        UTILASSERT(scaleXNode);
        float scaleX = std::stof(scaleXNode->value());

        xml_node<char> *scaleYNode = scaleNode->first_node("Y");
        UTILASSERT(scaleYNode);
        float scaleY = std::stof(scaleYNode->value());

        xml_node<char> *zDepthNode = node->first_node("ZDepth");
        float zDepth = 1.0f;

        if (zDepthNode) {
            zDepth = std::stof(zDepthNode->value());
        }

        element = new UI::Panel(sharedContext, glm::vec3(colorR, colorG, colorB), glm::vec2(positionX, positionY), glm::vec2(scaleX, scaleY), zDepth);

        if (fitTypeNode && std::string(fitTypeNode->value()) == "FIT_CHILDREN") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::FIT_CHILDREN;
        } else if (fitTypeNode && std::string(fitTypeNode->value()) == "NONE") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::NONE;
        }
    } else if (nodeName == "Label") {
        xml_node<char> *textNode = node->first_node("Text");
        UTILASSERT(textNode);
        std::string text = textNode->value();

        xml_node<char> *positionNode = node->first_node("Position");
        UTILASSERT(positionNode);

        xml_node<char> *positionXNode = positionNode->first_node("X");
        UTILASSERT(positionXNode);
        float positionX = std::stof(positionXNode->value());

        xml_node<char> *positionYNode = positionNode->first_node("Y");
        UTILASSERT(positionYNode);
        float positionY = std::stof(positionYNode->value());


        xml_node<char> *fontNode = node->first_node("Font");
        UTILASSERT(fontNode);
        std::string fontPath = fontNode->value();

        xml_node<char> *zDepthNode = node->first_node("ZDepth");
        float zDepth = 1.0f;

        if (zDepthNode) {
            zDepth = std::stof(zDepthNode->value());
        }

        element = new UI::Label(sharedContext, text, fontPath, glm::vec2(positionX, positionY), zDepth);
    } else if (nodeName == "Button") {
        xml_node<char> *fitTypeNode = node->first_node("FitType");

        xml_node<char> *positionNode = node->first_node("Position");
        UTILASSERT(positionNode);

        xml_node<char> *positionXNode = positionNode->first_node("X");
        UTILASSERT(positionXNode);
        float positionX = std::stof(positionXNode->value());

        xml_node<char> *positionYNode = positionNode->first_node("Y");
        UTILASSERT(positionYNode);
        float positionY = std::stof(positionYNode->value());


        xml_node<char> *scaleNode = node->first_node("Scale");
        UTILASSERT(scaleNode);

        xml_node<char> *scaleXNode = scaleNode->first_node("X");
        UTILASSERT(scaleXNode);
        float scaleX = std::stof(scaleXNode->value());

        xml_node<char> *scaleYNode = scaleNode->first_node("Y");
        UTILASSERT(scaleYNode);
        float scaleY = std::stof(scaleYNode->value());

        /* bgPanel and fgLabel */
        xml_node<char> *bgPanelNode = node->first_node("BgPanel");
        UTILASSERT(bgPanelNode);
        xml_node<char> *panelNode = bgPanelNode->first_node("Panel");

        GenericElement *bgPanel = DeserializeUIElement(sharedContext, panelNode);
        UTILASSERT(bgPanel->type == UI::PANEL);


        xml_node<char> *fgLabelNode = node->first_node("FgLabel");
        UTILASSERT(fgLabelNode);
        xml_node<char> *labelNode = fgLabelNode->first_node("Label");

        GenericElement *fgLabel = DeserializeUIElement(sharedContext, labelNode);
        UTILASSERT(fgLabel->type == UI::LABEL);

        element = new UI::Button(glm::vec2(positionX, positionY), glm::vec2(scaleX, scaleY), reinterpret_cast<UI::Panel *>(bgPanel), reinterpret_cast<UI::Label *>(fgLabel));

        /* FIT_CHILDREN is the default for buttons. */
        if (!fitTypeNode || std::string(fitTypeNode->value()) == "FIT_CHILDREN") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::FIT_CHILDREN;
        } else if (fitTypeNode && std::string(fitTypeNode->value()) == "NONE") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::NONE;
        }
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
