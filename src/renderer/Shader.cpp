#include "renderer/baseRenderer.hpp"
#include <renderer/Shader.hpp>
#include <vulkan/vulkan_core.h>

static std::vector<std::byte> readFile(const std::string &name) {
    std::ifstream file(name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to read " + name + "!");
    }

    size_t fileSize = file.tellg();
    std::vector<std::byte> output(file.tellg());

    file.seekg(0);
    file.read(reinterpret_cast<char *>(output.data()), fileSize);

    return output;
}

Shader::Shader(BaseRenderer *renderer, VkShaderStageFlagBits shaderStageBits, std::optional<std::string> shaderName) : m_Renderer(renderer), m_ShaderStageBits(shaderStageBits) {
    if (shaderName.has_value()) {
        LoadFromFile(shaderName.value());
    }
};

void Shader::LoadFromFile(const std::string &path) {
    std::vector<std::byte> code = readFile(path);

    m_RawShaderModule = m_Renderer->CreateShaderModule(code);
}

std::any Shader::GetShaderModule() const {
    return m_RawShaderModule;
}

VkShaderStageFlagBits Shader::GetShaderStageBits() const {
    return m_ShaderStageBits;
}
