#ifndef _MATERIAL_HPP_
#define _MATERIAL_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"


#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>

class Material {
public:
    Material() = default;

    void SetColor(const glm::vec3 &color);
    const glm::vec3 &GetColor() const;
private:
    glm::vec3 m_Color = glm::vec3(0.8);
};

class PBRMaterial : public Material {
public:
    PBRMaterial() = default;

    void SetMetallicFactor(const float &metallic);
    const float &GetMetallicFactor() const;

    void SetRoughnessFactor(const float &roughness);
    const float &GetRoughnessFactor() const;
private:
    float m_MetallicFactor = 0.0f;
    float m_RoughnessFactor = 0.0f;
};

#endif // _MATERIAL_HPP_
