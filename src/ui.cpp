#include "ui.hpp"
#include "common.hpp"
#include <vulkan/vulkan_core.h>

using namespace UI;

Panel::~Panel() {
    vkDeviceWaitIdle(m_SharedContext.engineDevice);

    vkDestroyBuffer(m_SharedContext.engineDevice, vertex2DBuffer.buffer, NULL);
    vkFreeMemory(m_SharedContext.engineDevice, vertex2DBuffer.memory, NULL);

    vkDestroyImage(m_SharedContext.engineDevice, texture.imageAndMemory.image, NULL);
    vkFreeMemory(m_SharedContext.engineDevice, texture.imageAndMemory.memory, NULL);
};

Panel::Panel(EngineSharedContext &sharedContext, glm::vec3 color, glm::vec4 dimensions) : m_SharedContext(sharedContext) {
    texture = CreateSinglePixelImage(sharedContext, color);

    dimensions *= 2;
    dimensions -= 1;

    vertex2DBuffer = CreateVertex2DBuffer(sharedContext, {
                                                            {glm::vec2(dimensions.x, dimensions.y), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec2(dimensions.x + dimensions.z, dimensions.y + dimensions.w), glm::vec2(1.0f, 1.0f)},
                                                            {glm::vec2(dimensions.x, dimensions.y + dimensions.w), glm::vec2(0.0f, 1.0f)},
                                                            {glm::vec2(dimensions.x, dimensions.y), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec2(dimensions.x + dimensions.z, dimensions.y), glm::vec2(1.0f, 0.0f)},
                                                            {glm::vec2(dimensions.x + dimensions.z, dimensions.y + dimensions.w), glm::vec2(1.0f, 1.0f)}
                                                        });
}