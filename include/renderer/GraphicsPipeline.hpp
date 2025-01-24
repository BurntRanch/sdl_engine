#pragma once

#include "renderer/DescriptorLayout.hpp"
#include "renderer/baseRenderer.hpp"
#include <SDL3/SDL_stdinc.h>
#include <string>
#include <vulkan/vulkan_core.h>
#include <any>

class GraphicsPipeline {
public:
    GraphicsPipeline(std::any rawPipeline, std::any rawPipelineLayout, DescriptorLayout descriptorLayout, BaseRenderer *renderer, glm::vec4 viewport, glm::vec4 scissor);

    std::any GetRawPipeline();
    std::any GetRawPipelineLayout();
    DescriptorLayout GetDescriptorLayout();
    glm::vec4 GetViewport();
    glm::vec4 GetScissor();
    BaseRenderer *GetRenderer();

    std::any GetBindingValue(Uint32 index);

    void SetRawPipeline(std::any rawPipeline);
    void SetRawPipelineLayout(std::any rawPipelineLayout);
    void SetDescriptorLayout(DescriptorLayout layout);
    void SetViewport(glm::vec4 viewport);
    void SetScissor(glm::vec4 scissor);
    void SetRenderFunction(const std::function<void(GraphicsPipeline *)> &func);
    void SetRenderer(BaseRenderer *renderer);

    void UpdateBindingValue(Uint32 index, std::any value);

    void Execute();
private:
    BaseRenderer *m_Renderer;

    std::any m_RawPipeline, m_RawPipelineLayout;

    DescriptorLayout m_Layout;

    glm::vec4 m_Viewport, m_Scissor;

    std::function<void(GraphicsPipeline *)> m_RenderFunction;

    std::vector<std::any> m_BindingValues;
};
