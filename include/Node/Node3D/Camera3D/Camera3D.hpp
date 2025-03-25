#ifndef _CAMERA3D_HPP_
#define _CAMERA3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "model.hpp"
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
    Camera3D(const float pitch, const float yaw, const float roll, const float near = 0.1f, const float far = 100.0f, const float FOV = 90.0f);

    Camera3D(const float pitch, const float yaw, const float roll, const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale);

    void SetNear(const float near);
    float GetNear();

    void SetFar(const float far);
    float GetFar();

    void SetFOV(const float FOV);
    float GetFOV();

    void SetEulerAngles(const glm::vec3 &eulerAngles);

    void SetPitch(const float pitch);
    float GetPitch();
    
    void SetYaw(const float yaw);
    float GetYaw();

    void SetRoll(const float roll);
    float GetRoll();
protected:
    /* This is here to avoid having to translate from m_Rotation. Although m_Rotation does get changed. */
    glm::vec3 m_EulerRotation;

    float m_Near = 0.1f;
    float m_Far = 100.0f;

    float m_FOV = 90.0f;
};

#endif // _CAMERA3D_HPP_