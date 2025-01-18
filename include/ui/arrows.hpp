#pragma once

#include "common.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>

namespace UI {
class Arrows : public GenericElement {
public:
    Object *arrowsObject;
    Object *highlightedObject;

    virtual ~Arrows() = default;

    Arrows(Object &highlightedModel);

    void SetPosition(glm::vec2) { throw std::runtime_error("SetPosition is not to be called on UI::Arrows objects, Modify the object that it highlights directly and the Arrow will follow it!"); };
    glm::vec2 GetPosition() { throw std::runtime_error("GetPosition is not to be called on UI::Arrows objects, Read the position from the object that it highlights directly."); };
};
}