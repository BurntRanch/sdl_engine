#pragma once
#include "common.hpp"
#include "renderer/vulkanRenderer.hpp"
#include <glm/glm.hpp>

namespace UI {
class GenericElement;

class Panel : public Scalable {
public:
    TextureImageAndMemory texture;
    
    ~Panel();

    /* Dimensions are expected to be provided as a 4D vector, {X, Y, W, H}. */
    Panel(VulkanRendererSharedContext &sharedContext, glm::vec3 color, glm::vec2 position, glm::vec2 scales, float zDepth);

    void SetPosition(glm::vec2 position);
    void SetScale(glm::vec2 scale);
    glm::vec2 GetUnfitScale();

    glm::vec2 GetPosition();
    glm::vec2 GetScale();

    glm::vec4 GetDimensions();

    void DestroyBuffers();
private:
    struct VulkanRendererSharedContext m_SharedContext;

    glm::vec4 m_Dimensions;
};
}