#include "Node/Node3D/Light3D/PointLight3D/PointLight3D.hpp"
#include "Node/Node3D/Node3D.hpp"

PointLight3D::PointLight3D(float constant, float linear, float quadratic) {
    SetAttenuation(constant, linear, quadratic);
}
PointLight3D::PointLight3D(float constant, float linear, float quadratic, const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) : Light3D(position, rotation, scale) {
    SetAttenuation(constant, linear, quadratic);
}

void PointLight3D::SetAttenuation(const float constant, const float linear, const float quadratic) {
    m_Attenuation[0] = constant;
    m_Attenuation[1] = linear;
    m_Attenuation[2] = quadratic;
}

glm::vec3 PointLight3D::GetAttenuation() {
    return m_Attenuation;
}
