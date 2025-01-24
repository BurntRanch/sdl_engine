#include "renderer/baseRenderer.hpp"
#include <renderer/DescriptorLayout.hpp>

DescriptorLayout::DescriptorLayout(BaseRenderer *renderer) {
    m_Renderer = renderer;
}

void DescriptorLayout::AddBinding(PipelineBinding binding) {
    m_Bindings.push_back(binding);
}

std::vector<PipelineBinding> DescriptorLayout::GetBindings() {
    return m_Bindings;
}

/* After calling create, it will lock all of the bindings in, and you will not be able to add any more. */
std::any DescriptorLayout::Create() {
    return m_RawLayout = m_Renderer->CreateDescriptorLayout(m_Bindings);
}

std::any DescriptorLayout::Get() const {
    return m_RawLayout;
}
