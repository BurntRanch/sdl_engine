#pragma once

#include "common.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>

namespace UI {
class Arrows : public GenericElement {
public:
    Model *arrowsModel;
    Model *highlightedModel;

    virtual ~Arrows() = default;

    Arrows(Model &highlightedModel);

    inline void SetPosition(glm::vec3 position) { throw std::runtime_error("SetPosition is not to be called on UI::Arrows objects, Modify the model that it highlights directly and the Arrow will follow it!"); };

    inline glm::vec2 GetPosition() { throw std::runtime_error("GetPosition is not to be called on UI::Arrows objects, Read the position from the model that it highlights directly."); };
};
}