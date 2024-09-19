#ifndef PARTICLES_HPP
#define PARTICLES_HPP

#include "common.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>

namespace UI {
class Waypoint : public GenericElement {
public:
    virtual ~Waypoint() = default;

    Waypoint(glm::vec3 position, float zDepth, glm::vec3 scale) : m_Position(position) { SetDepth(zDepth); };

    inline void SetPosition(glm::vec3 position) { m_Position = position; };

    inline glm::vec3 GetWorldSpacePosition() { return m_Position; };

    inline glm::vec2 GetPosition() { throw std::runtime_error("Use GetWorldSpacePosition() for UI::Waypoint objects!"); };
private:
    glm::vec3 m_Position;
};
}

#endif