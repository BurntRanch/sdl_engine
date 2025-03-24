#ifndef _MATERIAL_HPP_
#define _MATERIAL_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "model.hpp"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>

class Material {
public:
    ~Material();

    Material();

    void SetColor(glm::vec3 color);
    glm::vec3 GetColor();
private:
    glm::vec3 m_Color;
};

class PBRMaterial : public Material {
public:
    ~PBRMaterial();

    PBRMaterial();

    void SetMetallicFactor(float metallic);
    float GetMetallicFactor();

    void SetRoughnessFactor(float roughness);
    float GetRoughnessFactor();
private:
    float m_MetallicFactor = 0.0f;
    float m_RoughnessFactor = 0.0f;
};

#endif // _MATERIAL_HPP_
