#ifndef _POINTLIGHT3D_HPP_
#define _POINTLIGHT3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "Node/Node3D/Light3D/Light3D.hpp"
#include "camera.hpp"
#include "model.hpp"
#include <functional>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <material.hpp>

class PointLight3D : public Light3D {
public:
    PointLight3D(float constant, float linear, float quadratic);
    PointLight3D(float constant, float linear, float quadratic, const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale);

    void SetAttenuation(float constant, float linear, float quadratic);

    /* [0] = constant, [1] = linear, [2] = quadratic. */
    glm::vec3 GetAttenuation();
protected:
    glm::vec3 m_Attenuation;
};

#endif // _POINTLIGHT3D_HPP_