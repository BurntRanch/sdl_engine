#pragma once
#include "common.hpp"
#include <glm/glm.hpp>

namespace UI {
class Panel {
public:
    TextureImageAndMemory texture;
    BufferAndMemory vertex2DBuffer;
    
    ~Panel();

    /* Dimensions are expected to be provided as a 4D vector, {X, Y, W, H}. */
    Panel(struct EngineSharedContext &sharedContext, glm::vec3 color, glm::vec4 dimensions);
private:
    struct EngineSharedContext m_SharedContext;
};
}