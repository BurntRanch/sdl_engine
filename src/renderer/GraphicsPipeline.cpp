#include "renderer/DescriptorLayout.hpp"
#include "renderer/baseRenderer.hpp"
#include <renderer/GraphicsPipeline.hpp>

GraphicsPipeline::GraphicsPipeline(std::any rawPipeline, std::any rawPipelineLayout, DescriptorLayout layout, BaseRenderer *renderer, glm::vec4 viewport, glm::vec4 scissor) : m_Layout(renderer) {
    SetRenderer(renderer);
    SetRawPipeline(rawPipeline);
    SetRawPipelineLayout(rawPipelineLayout);
    SetDescriptorLayout(layout);
    SetViewport(viewport);
    SetScissor(scissor);
}

std::any GraphicsPipeline::GetRawPipeline() {
    return m_RawPipeline;
}
std::any GraphicsPipeline::GetRawPipelineLayout() {
    return m_RawPipelineLayout;
}
DescriptorLayout GraphicsPipeline::GetDescriptorLayout() {
    return m_Layout;
}
glm::vec4 GraphicsPipeline::GetViewport() {
    return m_Viewport;
}
glm::vec4 GraphicsPipeline::GetScissor() {
    return m_Scissor;
}
BaseRenderer *GraphicsPipeline::GetRenderer() {
    return m_Renderer;
}

std::any GraphicsPipeline::GetBindingValue(Uint32 index) {
    return m_BindingValues[index];
}

void GraphicsPipeline::SetRawPipeline(std::any rawPipeline) {
    m_RawPipeline = rawPipeline;
}
void GraphicsPipeline::SetRawPipelineLayout(std::any rawPipelineLayout) {
    m_RawPipelineLayout = rawPipelineLayout;
}
void GraphicsPipeline::SetDescriptorLayout(DescriptorLayout layout) {
    m_Layout = layout;
    m_BindingValues.resize(m_Layout.GetBindings().size());
}
void GraphicsPipeline::SetViewport(glm::vec4 viewport) {
    m_Viewport = viewport;
}
void GraphicsPipeline::SetScissor(glm::vec4 scissor) {
    m_Scissor = scissor;
}
void GraphicsPipeline::SetRenderFunction(const std::function<void(GraphicsPipeline *)> &func) {
    m_RenderFunction = func;
}
void GraphicsPipeline::SetRenderer(BaseRenderer *renderer) {
    m_Renderer = renderer;
}

void GraphicsPipeline::UpdateBindingValue(Uint32 index, std::any value) {
    if (m_BindingValues.size() < index) {
        m_BindingValues.resize(index);
    }

    m_BindingValues.at(index) = value;
}

void GraphicsPipeline::Execute() {
    m_Renderer->BeginPipeline(this);

    if (m_RenderFunction) {
        m_RenderFunction(this);
    }
}
