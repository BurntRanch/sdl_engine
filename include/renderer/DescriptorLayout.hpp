#pragma once

#include "renderer/baseRenderer.hpp"
#include <SDL3/SDL_stdinc.h>
#include <any>
#include <vector>
#include <vulkan/vulkan_core.h>

struct PipelineBinding {
    VkDescriptorType type;
    VkShaderStageFlagBits shaderStageBits;
    Uint32 bindingIndex; /* TODO: OpenGL */
};

class DescriptorLayout {
public:
    DescriptorLayout(BaseRenderer *renderer);

    void AddBinding(PipelineBinding binding);
    std::vector<PipelineBinding> GetBindings();

    /* After calling create, it will lock all of the bindings in, and you will not be able to add any more. */
    std::any Create();

    std::any Get() const;
private:
    BaseRenderer *m_Renderer;

    std::any m_RawLayout;

    std::vector<PipelineBinding> m_Bindings;
};
