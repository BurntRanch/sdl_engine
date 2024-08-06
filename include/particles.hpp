#ifndef PARTICLES_HPP
#define PARTICLES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

class Particle {
public:

    Particle() = default;
    Particle(glm::vec3 position, glm::vec3 scale) : m_Position(position), m_BoundingBox({ scale, -scale }) {};

    constexpr void SetPosition(glm::vec3 position) { m_Position = position; };
    constexpr void SetScale(glm::vec3 scale) { m_BoundingBox = {scale, -scale}; };

    constexpr glm::vec3 GetPosition() { return m_Position; };
    constexpr std::array<glm::vec3, 2> GetBoundingBox() { return m_BoundingBox; };
private:
    glm::vec3 m_Position;

    std::array<glm::vec3, 2> m_BoundingBox;
};

#endif