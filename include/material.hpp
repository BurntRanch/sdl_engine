#ifndef _MATERIAL_HPP_
#define _MATERIAL_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"


#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <optional>

class Material {
public:
    virtual ~Material() {};

    virtual void SetColor(const glm::vec3 &color);
    virtual const glm::vec3 &GetColor() const;

    /* Textures are in the form of filenames, It is the renderer's responsibility to load them into whatever Graphics API it's using. */
    virtual void SetTexturePath(const std::string &texturePath);
    virtual const std::string &GetTexturePath() const;
private:
    glm::vec3 m_Color = glm::vec3(0.8);

    std::string m_TexturePath = "";
};

class PBRMaterial : public Material {
public:
    virtual ~PBRMaterial() {};

    PBRMaterial() : Material() {};

    void SetMetallicFactor(const float &metallic);
    const float &GetMetallicFactor() const;

    void SetRoughnessFactor(const float &roughness);
    const float &GetRoughnessFactor() const;
private:
    float m_MetallicFactor = 0.0f;
    float m_RoughnessFactor = 0.0f;
};

#endif // _MATERIAL_HPP_
