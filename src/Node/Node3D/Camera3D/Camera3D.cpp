#include "Node/Node3D/Node3D.hpp"
#include <Node/Node3D/Camera3D/Camera3D.hpp>
#include <glm/gtc/quaternion.hpp>

Camera3D::Camera3D(const float pitch, const float yaw, const float roll, const float near, const float far, const float FOV) {
    SetPitch(pitch);
    SetYaw(yaw);
    SetRoll(roll);
}

Camera3D::Camera3D(const float pitch, const float yaw, const float roll, const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) : Node3D(position, rotation, scale) {
    SetPitch(pitch);
    SetYaw(yaw);
    SetRoll(roll);
}

void Camera3D::SetNear(const float near) {
    m_Near = near;
}
float Camera3D::GetNear() {
    return m_Near;
}

void Camera3D::SetFar(const float far) {
    m_Far = far;
}
float Camera3D::GetFar() {
    return m_Far;
}

void Camera3D::SetFOV(const float FOV) {
    m_FOV = FOV;
}
float Camera3D::GetFOV() {
    return m_FOV;
}

void Camera3D::SetEulerAngles(const glm::vec3 &eulerAngles) {
    m_EulerRotation = eulerAngles;
    m_Rotation = m_EulerRotation;
}

void Camera3D::SetPitch(const float pitch) {
    m_EulerRotation.x = pitch;
    m_Rotation = m_EulerRotation;
}
float Camera3D::GetPitch() {
    return m_EulerRotation.x;
}

void Camera3D::SetYaw(const float yaw) {
    m_EulerRotation.y = yaw;
    m_Rotation = m_EulerRotation;
}
float Camera3D::GetYaw() {
    return m_EulerRotation.y;
}

void Camera3D::SetRoll(const float roll) {
    m_EulerRotation.z = roll;
    m_Rotation = m_EulerRotation;
}
float Camera3D::GetRoll() {
    return m_EulerRotation.z;
}
