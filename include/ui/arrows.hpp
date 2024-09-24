#pragma once

#include "common.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>

namespace UI {
class Arrows : public GenericElement {
public:
    Model *model;

    virtual ~Arrows() = default;

    Arrows(glm::vec3 position);

    inline void SetPosition(glm::vec3 position) { m_NeedsUpdate = true; m_Position = position; };

    inline glm::vec3 GetWorldSpacePosition() { return m_Position; };

    inline glm::vec2 GetPosition() { throw std::runtime_error("Use GetWorldSpacePosition() for UI::Arrows objects!"); };

    glm::mat4 GetModelMatrix();
private:
    bool m_NeedsUpdate = true;

    glm::vec3 m_Position;

    glm::mat4 m_ModelMatrix;
};
}