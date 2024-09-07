#ifndef PARTICLES_HPP
#define PARTICLES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace UI {
class Waypoint {
public:
    Waypoint() = default;
    Waypoint(glm::vec3 position, glm::vec3 scale) : m_Position(position) {};

    constexpr void SetPosition(glm::vec3 position) { m_Position = position; };

    constexpr glm::vec3 GetPosition() { return m_Position; };
private:
    glm::vec3 m_Position;
};
}

#endif