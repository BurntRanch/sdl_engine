#include <glm/ext/vector_float2.hpp>
#include <vector>
#include "common.hpp"
#include "renderer/vulkanRenderer.hpp"

namespace UI {
    std::vector<GenericElement *> LoadUIFile(BaseRenderer *renderer, std::string_view fileName);
}

#include <ui/panel.hpp>
#include <ui/label.hpp>
#include <ui/waypoint.hpp>
#include <ui/arrows.hpp>