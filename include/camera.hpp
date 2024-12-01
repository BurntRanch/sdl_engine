#ifndef CAMERA_HPP
#define CAMERA_HPP

#define CAMERA_NEAR 0.1f
#define CAMERA_FAR 100.0f

// From learnopengl.com

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// Default camera values
const float YAW         =  0.0f;
const float PITCH       =  0.0f;
const float SPEED       =  5.0f;
const float SENSITIVITY =  1.0f;
const float FIELDOFVIEW =  90.0f;


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // euler Angles
    float Yaw;
    float Pitch;
    // camera options
    float MovementSpeed;
    float MouseSensitivity;
    float FOV;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f), float yaw = YAW, float pitch = PITCH);

    // constructor with scalar values
    Camera(float posX = 0.0f, float posY = 0.0f, float posZ = 0.0f, float upX = 0.0f, float upY = 0.0f, float upZ = 1.0f, float yaw = YAW, float pitch = PITCH);

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix() const;

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(const Camera_Movement& direction, float deltaTime);

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainYaw = true);

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset);
private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    inline void updateCameraVectors()
    {
        // calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Pitch)) * cos(glm::radians(Yaw));
        front.y = sin(glm::radians(Pitch)) * cos(glm::radians(Yaw));
        front.z = sin(glm::radians(Yaw));
        Front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};

#endif
