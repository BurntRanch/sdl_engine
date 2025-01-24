#pragma once

#include "renderer/baseRenderer.hpp"
#include <string>
#include <vulkan/vulkan_core.h>

class Shader {
public:
    Shader(BaseRenderer *renderer, VkShaderStageFlagBits shaderStageBits, std::optional<std::string> shaderName = {});

    void LoadFromFile(const std::string &path);

    /* This is only valid if it was loaded from a file prior to this. */
    std::any GetShaderModule() const;

    VkShaderStageFlagBits GetShaderStageBits() const;
private:
    BaseRenderer *m_Renderer;

    /* this is an any object, because its interpretation is up to the derived renderer. */
    std::any m_RawShaderModule;

    VkShaderStageFlagBits m_ShaderStageBits;
};