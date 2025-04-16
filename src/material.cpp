#include "material.hpp"

void Material::SetColor(const glm::vec3 &color) {
    m_Color = color;
}
const glm::vec3 &Material::GetColor() const {
    return m_Color;
}

void Material::SetTexturePath(const std::string &texturePath) {
    m_TexturePath = texturePath;
}
const std::string &Material::GetTexturePath() const {
    return m_TexturePath;
}

void PBRMaterial::SetMetallicFactor(const float &metallic) {
    m_MetallicFactor = metallic;
}
const float &PBRMaterial::GetMetallicFactor() const {
    return m_MetallicFactor;
}

void PBRMaterial::SetRoughnessFactor(const float &roughness) {
    m_RoughnessFactor = roughness;
}
const float &PBRMaterial::GetRoughnessFactor() const {
    return m_RoughnessFactor;
}
