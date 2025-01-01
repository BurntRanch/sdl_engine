#include "camera.hpp"
#include "object.hpp"

Camera::Camera(glm::vec3 up, float yaw, float pitch) : Front(glm::vec3(0.0f, 1.0f, 0.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), FOV(FIELDOFVIEW)
{
    HighestCameraID++;
    SetCameraID(HighestCameraID);

    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

// constructor with scalar values
Camera::Camera(float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 1.0f, 0.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), FOV(FIELDOFVIEW)
{
    WorldUp = glm::vec3(upX, upY, upZ);
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

// returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 Camera::GetViewMatrix() const
{
    glm::vec3 position = (m_ObjectAttachment != nullptr ? m_ObjectAttachment->GetPosition() : glm::vec3(0.0f, 0.0f, 0.0f));
    return glm::lookAt(position, position + Front, Up);
}

// // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
// void Camera::ProcessKeyboard(const Camera_Movement& direction, float deltaTime)
// {
//     float velocity = MovementSpeed * deltaTime;
//     if (direction == FORWARD)
//         Position += Front * velocity;
//     if (direction == BACKWARD)
//         Position -= Front * velocity;
//     if (direction == LEFT)
//         Position -= Right * velocity;
//     if (direction == RIGHT)
//         Position += Right * velocity;
// }

// // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
// void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainYaw)
// {
//     xoffset *= MouseSensitivity;
//     yoffset *= MouseSensitivity;

//     Pitch -= xoffset;
//     Yaw   -= yoffset;

//     // make sure that when yaw is out of bounds, screen doesn't get flipped
//     if (constrainYaw)
//     {
//         if (Yaw > 89.0f)
//             Yaw = 89.0f;
//         if (Yaw < -89.0f)
//             Yaw = -89.0f;
//     }

//     // update Front, Right and Up Vectors using the updated Euler angles
//     updateCameraVectors();
// }

// // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
// void Camera::ProcessMouseScroll(float yoffset)
// {
//     FOV -= (float)yoffset;
//     if (FOV < 1.0f)
//         FOV = 1.0f;
//     if (FOV > 45.0f)
//         FOV = 45.0f;
// }


void Camera::SetCameraID(int cameraID) {
    m_CameraID = cameraID;
}

int Camera::GetCameraID() {
    return m_CameraID;
}

void Camera::SetObjectAttachment(Object *obj) {
    if (m_ObjectAttachment == obj) {
        return;
    }

    Object *oldObjectAttachment = m_ObjectAttachment;

    m_ObjectAttachment = obj;

    if (oldObjectAttachment != nullptr) {
        oldObjectAttachment->SetCameraAttachment(nullptr);
    }

    if (obj != nullptr) {
        obj->SetCameraAttachment(this);
    }
}

int Camera::HighestCameraID = -1;
