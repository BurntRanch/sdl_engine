#include "util.hpp"
#include "camera.hpp"
#include "engine.hpp"
#include "isteamnetworkingsockets.h"
#include "rapidxml.hpp"
#include "ui/label.hpp"
#include "common.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

bool endsWith(const std::string_view fullString, const std::string_view ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

std::vector<std::string> split(const std::string_view text, const char delim) {
    std::string                 line;
    std::vector<std::string>    vec;
    std::stringstream ss(text.data());
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}

Object *DeepSearchObjectTree(Object *obj, std::function<bool(Object *)> pred) {
    for (Object *child : obj->GetChildren()) {
        if (pred(child)) {
            return child;
        }

        Object *deepSearchResult = DeepSearchObjectTree(child, pred);
        if (deepSearchResult != nullptr) {
            return deepSearchResult;
        }
    }

    return nullptr;
}

std::vector<std::pair<Networking_Object *, int>> FilterRelatedNetworkingObjects(std::vector<Networking_Object> &candidates, Networking_Object *object) {
    std::vector<std::pair<Networking_Object *, int>> relatedObjects;

    for (size_t i = 0; i < candidates.size(); i++) {
        if (std::find(object->children.begin(), object->children.end(), candidates[i].ObjectID) != object->children.end()) {
            relatedObjects.push_back(std::make_pair(&candidates[i], i));

            /* Go over any candidates that we may have missed. */
            std::vector<Networking_Object> previousCandidates{candidates.begin(), candidates.begin() + i};

            std::vector<std::pair<Networking_Object *, int>> relatedPreviousCandidates = FilterRelatedNetworkingObjects(previousCandidates, &candidates[i]);

            if (!relatedPreviousCandidates.empty()) {
                relatedObjects.insert(relatedObjects.end(), relatedPreviousCandidates.begin(), relatedPreviousCandidates.end());
            }

            continue;
        }
    }

    return relatedObjects;
}

bool intersects(const glm::vec3 &origin, const glm::vec3 &front, const std::array<glm::vec3, 2> &boundingBox) {
    const glm::vec3 inverse_front = 1.0f / front;

    const glm::vec3 &box_max = boundingBox[0];
    const glm::vec3 &box_min = boundingBox[1];

    const float t1 = (box_min.x - origin.x) * inverse_front.x;
    const float t2 = (box_max.x - origin.x) * inverse_front.x;

    float t_near = std::max(CAMERA_NEAR, glm::min(t1, t2));
    float t_far  = std::min(CAMERA_FAR,  glm::max(t1, t2));

    const float t3 = (box_min.y - origin.y) * inverse_front.y;
    const float t4 = (box_max.y - origin.y) * inverse_front.y;

    t_near = glm::max(t_near, glm::min(t3, t4));
    t_far  = glm::min(t_far,  glm::max(t3, t4));

    return t_near <= t_far && t_far >= 0;
}

glm::vec2 adjustScaleToFitType(UI::Scalable *self, glm::vec2 scale, UI::FitType fitType) {
    if (fitType == UI::UNSET)
        fitType = self->fitType;
    
    UI::Scalable *element = self;   // Used in UNSET handling
    switch (fitType) {
        case UI::UNSET:
            while (element->GetParent() && element->GetParent()->genericType == UI::SCALABLE) {
                element = reinterpret_cast<UI::Scalable *>(element->GetParent());

                if (element->fitType == UI::UNSET) {
                    continue;
                }

                scale = adjustScaleToFitType(self, scale, element->fitType);
            }
        case UI::NONE:
            break;
        case UI::FIT_CHILDREN:
            for (UI::GenericElement *child : self->GetChildren()) {
                if (child->genericType != UI::SCALABLE && child->genericType != UI::LABEL) {
                    continue;
                }

                if (child->genericType == UI::LABEL) {
                    scale = glm::max(scale, reinterpret_cast<UI::Label *>(child)->CalculateMinimumScaleToFit());
                    continue;
                }

                scale = glm::max(scale, reinterpret_cast<UI::Scalable *>(child)->GetScale());
            }
            
            break;
    }

    return scale;
}

rapidxml::xml_node<char>* getPropertiesNode(rapidxml::xml_node<char> *uiObjectNode) {
    using rapidxml::xml_node;

    xml_node<char> *propertiesNode = uiObjectNode->first_node("Properties");
    UTILASSERT(propertiesNode);

    return propertiesNode;
};

std::string getID(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *idNode = propertiesNode->first_node("ID");
    UTILASSERT(idNode);
    std::string id = idNode->value();

    return id;
}

glm::vec3 getColor(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *colorNode = propertiesNode->first_node("Color");
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

    return glm::vec3(colorR, colorG, colorB);
}

glm::vec2 getPosition(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *positionNode = propertiesNode->first_node("Position");
    UTILASSERT(positionNode);

    xml_node<char> *positionXNode = positionNode->first_node("X");
    UTILASSERT(positionXNode);
    float positionX = std::stof(positionXNode->value());

    xml_node<char> *positionYNode = positionNode->first_node("Y");
    UTILASSERT(positionYNode);
    float positionY = std::stof(positionYNode->value());

    return glm::vec2(positionX, positionY);
}

glm::vec2 getScale(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *scaleNode = propertiesNode->first_node("Scale");
    UTILASSERT(scaleNode);

    xml_node<char> *scaleXNode = scaleNode->first_node("X");
    UTILASSERT(scaleXNode);
    float scaleX = std::stof(scaleXNode->value());

    xml_node<char> *scaleYNode = scaleNode->first_node("Y");
    UTILASSERT(scaleYNode);
    float scaleY = std::stof(scaleYNode->value());

    return glm::vec2(scaleX, scaleY);
}

float getZDepth(rapidxml::xml_node<char> *propertiesNode, float depthDefault) {
    using rapidxml::xml_node;

    xml_node<char> *zDepthNode = propertiesNode->first_node("ZDepth");
    float zDepth = depthDefault;

    if (zDepthNode) {
        zDepth = std::stof(zDepthNode->value());
    }

    return zDepth;
}

bool getVisible(rapidxml::xml_node<char> *propertiesNode) {
    using rapidxml::xml_node;

    xml_node<char> *visibleNode = propertiesNode->first_node("Visible");

    if (!visibleNode) {
        return true;
    }

    std::string visibleString = std::string(visibleNode->value());

    std::transform(visibleString.begin(), visibleString.end(), visibleString.begin(), ::tolower);

    if (visibleString == "true") {
        return true;
    }

    return false;
}
