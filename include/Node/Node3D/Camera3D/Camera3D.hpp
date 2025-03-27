#ifndef _CAMERA3D_HPP_
#define _CAMERA3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"

#include <functional>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <Node/Node3D/Node3D.hpp>

/* interesting note: SetPitch, SetYaw, and SetRoll actually translate it to the quaternion rotation value.
 * It's a bad idea to edit rotation values, setting the pitch/yaw/roll will just overwrite it. and getting the pitch/yaw/roll won't be affected (if you change the rotation).
 */
class Camera3D : public Node3D {
public:
    Camera3D(const glm::vec3 up, const float near = 0.1f, const float far = 100.0f, const float FOV = 90.0f);

    Camera3D(const Node &node) : Node3D(node) {};
    Camera3D(const Node3D &node3D) : Node3D(node3D) {};

    Camera3D(const glm::vec3 up, const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale);

    void SetNear(const float near);
    const float &GetNear() const;

    void SetFar(const float far);
    const float &GetFar() const;

    void SetFOV(const float FOV);
    const float &GetFOV() const;

    void SetUp(const glm::vec3 up);
    const glm::vec3 &GetUp() const;

    const glm::mat4 GetViewMatrix();
protected:
    float m_Near = 0.1f;
    float m_Far = 100.0f;

    float m_FOV = 90.0f;

    glm::vec3 m_WorldUp;
    glm::vec3 m_Front, m_Up, m_Right;

private:
    void CalculateCameraVectors();
};

#endif // _CAMERA3D_HPP_