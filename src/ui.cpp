#include "ui.hpp"

#include "common.hpp"
#include "renderer/vulkanRenderer.hpp"
#include "ui/arrows.hpp"
#include "ui/button.hpp"
#include "ui/label.hpp"
#include "engine.hpp"
#include "util.hpp"

#include <SDL3/SDL_stdinc.h>
#include <filesystem>
#include <freetype/freetype.h>
#include <glm/ext/matrix_transform.hpp>
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include <rapidxml.hpp>

using namespace UI;

Panel::~Panel() {
    DestroyBuffers();
};

Panel::Panel(BaseRenderer *renderer, glm::vec3 color, glm::vec2 position, glm::vec2 scales, float zDepth)
    : m_Renderer(renderer) {

    genericType = SCALABLE;
    type = PANEL;
        
    texture = renderer->CreateSinglePixelImage(color);
    
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
        glm::vec2 parentScale = (m_Parent->genericType == SCALABLE ? reinterpret_cast<Scalable *>(m_Parent)->GetUnfitScale() : glm::vec4(1.0f));

        glm::vec4 dimensions = m_Dimensions;
        dimensions.x *= parentScale.x;
        dimensions.x += parentPosition.x;

        dimensions.y *= parentScale.y;
        dimensions.y += parentPosition.y;
        
        dimensions.z *= parentScale.x;
        dimensions.w *= parentScale.y;

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

glm::vec2 Panel::GetUnfitScale() {
    return glm::vec2(m_Dimensions.z, m_Dimensions.w) * (m_Parent != nullptr && m_Parent->genericType == SCALABLE ? reinterpret_cast<Scalable *>(m_Parent)->GetUnfitScale() : glm::vec3(1));
}

void Panel::DestroyBuffers() {
    m_Renderer->DestroyImage(texture.imageAndMemory);
}

Label::~Label() {
    DestroyBuffers();
}

Label::Label(BaseRenderer *renderer, std::string text, std::filesystem::path fontPath, glm::vec2 position, float zDepth)
    : m_Renderer(renderer) {

    genericType = LABEL;
    type = LABEL;
    
    SetPosition(position);
    SetDepth(m_Depth);

    InitGlyphs(text, fontPath);
}

void Label::InitGlyphs(std::string text, std::filesystem::path fontPath) {
    Glyphs.clear();

    if (FT_Init_FreeType(&m_FTLibrary)) {
        throw std::runtime_error("Failed to initialize FreeType!");
    }

    if (FT_New_Face(m_FTLibrary, fontPath.c_str(), 0, &m_FTFace)) {
        throw std::runtime_error(fmt::format("Failed to locate the requested font ({}) in your system!", fontPath.string()));
    }

    FT_Set_Pixel_Sizes(m_FTFace, 0, PIXEL_HEIGHT);

    float x = 0.0f;
    float y = 0.0f;

    for (char c : text) {
        Glyph glyph = m_Renderer->GenerateGlyph(m_FTFace, c, x, y, m_Depth);

        if (!glyph.glyphBuffer.has_value()) {
            continue;
        }

        Glyphs.push_back(glyph);
    }

    m_Text = text;
    m_FontPath = fontPath;

    /* It should return true if this label was added to the Renderer */
    if (m_Renderer->RemoveUILabel(this)) {
        m_Renderer->AddUILabel(this);
    }
}

void Label::SetText(std::string text) {
    InitGlyphs(text, m_FontPath);
}

void Label::SetFont(std::filesystem::path fontPath) {
    InitGlyphs(m_Text, fontPath);
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

Arrows::Arrows(Object &highlightedModel) {
    genericType = ARROWS;
    type = ARROWS;

    arrowsObject = new Object();
    arrowsObject->ImportFromFile("models/arrows.obj");
    arrowsObject->SetParent(&highlightedModel);

    arrowsObject->SetScale(glm::vec3(0.5f, 0.5f, 0.5f));
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
    glm::vec2 position = m_Position + (m_Parent == nullptr ? glm::vec2(0.0f, 0.0f) : m_Parent->GetPosition());

    if (genericType == SCALABLE && m_Parent != nullptr && m_Parent->genericType == SCALABLE) {
        position *= reinterpret_cast<Scalable *>(m_Parent)->GetUnfitScale();
    }

    return position;
}

inline void GenericElement::SetPosition(glm::vec2 Position) {
    m_Position = Position;
}

inline float GenericElement::GetDepth() {
    return m_Depth;
}

inline void GenericElement::SetVisible(bool visible) {
    m_Visible = visible;
}

inline bool GenericElement::GetVisible() {
    return m_Visible && (m_Parent ? m_Parent->GetVisible() : true);
}

inline void GenericElement::SetDepth(float depth) {
    m_Depth = depth*0.9f;   // 0.9f to avoid conflicting with the upscaled image which has a depth of 1.0
}

inline void GenericElement::SetParent(GenericElement *parent) {
    m_Parent = parent;

    if (parent == nullptr) {
        return;
    }

    if (genericType == SCALABLE && parent->genericType == SCALABLE && reinterpret_cast<Scalable *>(this)->fitType == UI::UNSET) {
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

Scalable::Scalable() {
    type = SCALABLE;
    genericType = SCALABLE;
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

    if (id == "bgPanel") {
        fmt::println("{} {} {} {}", m_Scale.x, m_Scale.y, reinterpret_cast<UI::Scalable *>(m_Parent)->GetUnfitScale().x, reinterpret_cast<UI::Scalable *>(m_Parent)->GetUnfitScale().y);
    }

    return scale;
}

GenericElement *DeserializeUIElement(BaseRenderer *renderer, rapidxml::xml_node<char> *node, UI::GenericElement *parent = nullptr) {
    using namespace rapidxml;
    std::string nodeName = node->name();

    GenericElement *element;

    xml_node<char> *propertiesNode;
    propertiesNode = getPropertiesNode(node);

    if (nodeName == "Group") {
        xml_node<char> *fitTypeNode = propertiesNode->first_node("FitType");

        glm::vec2 position = getPosition(propertiesNode);

        glm::vec2 scale = getScale(propertiesNode);

        float zDepth = getZDepth(propertiesNode);

        element = new UI::Scalable();

        element->SetPosition(position);
        element->SetDepth(zDepth);

        reinterpret_cast<UI::Scalable *>(element)->SetScale(scale);

        if (fitTypeNode && std::string(fitTypeNode->value()) == "FIT_CHILDREN") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::FIT_CHILDREN;
        } else if (fitTypeNode && std::string(fitTypeNode->value()) == "NONE") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::NONE;
        }
    } else if (nodeName == "Panel") {
        xml_node<char> *fitTypeNode = propertiesNode->first_node("FitType");
        
        glm::vec3 color = getColor(propertiesNode);

        glm::vec2 position = getPosition(propertiesNode);

        glm::vec2 scale = getScale(propertiesNode);

        float zDepth = getZDepth(propertiesNode);

        element = new UI::Panel(renderer, color, position, scale, zDepth);

        if (fitTypeNode && std::string(fitTypeNode->value()) == "FIT_CHILDREN") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::FIT_CHILDREN;
        } else if (fitTypeNode && std::string(fitTypeNode->value()) == "NONE") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::NONE;
        }
    } else if (nodeName == "Label") {
        xml_node<char> *textNode = propertiesNode->first_node("Text");
        UTILASSERT(textNode);
        std::string text = textNode->value();

        glm::vec2 position = getPosition(propertiesNode);

        xml_node<char> *fontNode = propertiesNode->first_node("Font");
        UTILASSERT(fontNode);
        std::string fontPath = fontNode->value();

        float zDepth = getZDepth(propertiesNode);

        element = new UI::Label(renderer, text, fontPath, position, zDepth);
    } else if (nodeName == "Button") {
        xml_node<char> *fitTypeNode = propertiesNode->first_node("FitType");

        glm::vec2 position = getPosition(propertiesNode);

        glm::vec2 scale = getScale(propertiesNode);

        /* bgPanel and fgLabel */
        xml_node<char> *bgPanelNode = propertiesNode->first_node("BgPanel");
        UTILASSERT(bgPanelNode);
        xml_node<char> *panelNode = bgPanelNode->first_node("Panel");

        GenericElement *bgPanel = DeserializeUIElement(renderer, panelNode);
        UTILASSERT(bgPanel->type == UI::PANEL);


        xml_node<char> *fgLabelNode = propertiesNode->first_node("FgLabel");
        UTILASSERT(fgLabelNode);
        xml_node<char> *labelNode = fgLabelNode->first_node("Label");

        GenericElement *fgLabel = DeserializeUIElement(renderer, labelNode);
        UTILASSERT(fgLabel->type == UI::LABEL);

        element = new UI::Button(position, scale, reinterpret_cast<UI::Panel *>(bgPanel), reinterpret_cast<UI::Label *>(fgLabel));

        /* FIT_CHILDREN is the default for buttons. */
        if (!fitTypeNode || std::string(fitTypeNode->value()) == "FIT_CHILDREN") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::FIT_CHILDREN;
        } else if (fitTypeNode && std::string(fitTypeNode->value()) == "NONE") {
            reinterpret_cast<UI::Scalable *>(element)->fitType = UI::NONE;
        }
    } else {
        fmt::println("WARN: Unknown UI Serialized Object Type: {}", nodeName);

        return nullptr;
    }

    element->id = getID(propertiesNode);
    element->SetVisible(getVisible(propertiesNode));

    for (xml_node<char> *childElement = node->first_node("Properties")->next_sibling(); childElement; childElement = childElement->next_sibling()) {
        DeserializeUIElement(renderer, childElement, element);
    }

    element->SetParent(parent);

    return element;
}

std::vector<GenericElement *> UI::LoadUIFile(BaseRenderer *renderer, std::string_view fileName) {
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
        GenericElement *element = DeserializeUIElement(renderer, uiElement);

        if (!element)
            continue;

        elements.push_back(element);
    }

    return elements;
}
