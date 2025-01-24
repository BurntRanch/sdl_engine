#include "renderer/GraphicsPipeline.hpp"
#include "renderer/baseRenderer.hpp"
#include <renderer/RenderPass.hpp>

RenderPass::RenderPass(BaseRenderer *renderer, std::any rawRenderPass, glm::vec2 resolution) {
    SetRenderer(renderer);
    SetRawRenderPass(rawRenderPass);
    SetResolution(resolution);
}

void RenderPass::SetClearColor(glm::vec4 clearColor) {
    m_ClearColor = clearColor;
}
glm::vec4 RenderPass::GetClearColor() {
    return m_ClearColor;
}

void RenderPass::SetSubpass(Uint32 index, GraphicsPipeline *pipeline) {
    if (m_Subpasses.size() <= index) {
        m_Subpasses.resize(index+1);
    }

    m_Subpasses[index] = pipeline;
}

void RenderPass::SetRawRenderPass(std::any rawRenderPass) {
    m_RawRenderPass = rawRenderPass;
}
void RenderPass::SetRenderer(BaseRenderer *renderer) {
    m_Renderer = renderer;
}
void RenderPass::SetResolution(glm::vec2 resolution) {
    m_Resolution = resolution;
}

std::any RenderPass::GetRawRenderPass() {
    return m_RawRenderPass;
}
BaseRenderer *RenderPass::GetRenderer() {
    return m_Renderer;
}
glm::vec2 RenderPass::GetResolution() {
    return m_Resolution;
}

void RenderPass::Execute(std::any rawFramebuffer) {
    m_Renderer->BeginRenderPass(this, rawFramebuffer);

    bool isFirstPass = true;
    for (auto &pipelinePass : m_Subpasses) {
        if (!isFirstPass) {
            m_Renderer->StartNextSubpass();
        }
        isFirstPass = false;

        if (pipelinePass.has_value()) {
            pipelinePass.value()->Execute();
        }
    }

    m_Renderer->EndRenderPass();
}
