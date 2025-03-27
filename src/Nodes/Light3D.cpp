#include "Node/Node3D/Light3D/Light3D.hpp"

void Light3D::SetLightColor(const glm::vec3 color) {
    m_LightColor = color;
}

const glm::vec3 &Light3D::GetLightColor() const {
    return m_LightColor;
}
