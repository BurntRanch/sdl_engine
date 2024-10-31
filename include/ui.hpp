#include <glm/ext/vector_float2.hpp>
#include <vector>
#include "common.hpp"

namespace UI {
    std::vector<GenericElement *> LoadUIFile(EngineSharedContext &sharedContext, std::string_view fileName);
}

#include <ui/panel.hpp>
#include <ui/label.hpp>
#include <ui/waypoint.hpp>
#include <ui/arrows.hpp>