#include "Node/Node3D/Node3D.hpp"
#include "fmt/base.h"
#include <Node/Node3D/Camera3D/Camera3D.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

Camera3D::Camera3D(const glm::vec3 up, const float near, const float far, const float FOV) {
    SetUp(up);
}

Camera3D::Camera3D(const glm::vec3 up, const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) : Node3D(position, rotation, scale) {
    SetUp(up);
}

void Camera3D::SetNear(const float near) {
    m_Near = near;
}
const float &Camera3D::GetNear() const {
    return m_Near;
}

void Camera3D::SetFar(const float far) {
    m_Far = far;
}
const float &Camera3D::GetFar() const {
    return m_Far;
}

void Camera3D::SetFOV(const float FOV) {
    m_FOV = FOV;
}
const float &Camera3D::GetFOV() const {
    return m_FOV;
}

void Camera3D::SetUp(const glm::vec3 up) {
    m_WorldUp = up;
}
const glm::vec3 &Camera3D::GetUp() const {
    return m_WorldUp;
}

const glm::mat4 Camera3D::GetViewMatrix() {
    CalculateCameraVectors();

    glm::vec3 position = GetAbsolutePosition();

    return glm::lookAt(position, position + m_Front, m_Up);
}

void Camera3D::CalculateCameraVectors() {
    glm::vec3 absoluteEulerRotation = glm::eulerAngles(GetAbsoluteRotation());
    
    float pitch = absoluteEulerRotation.x;
    float yaw = absoluteEulerRotation.y;

    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(pitch) * cos(yaw);
    front.y = sin(pitch) * cos(yaw);
    front.z = sin(yaw);
    m_Front = glm::normalize(front);
    // also re-calculate the Right and Up vector
    m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    m_Up    = glm::normalize(glm::cross(m_Right, m_Front));
}
