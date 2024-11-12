#pragma once
#include "common.hpp"
#include "ui/panel.hpp"
#include "ui/label.hpp"
#include <glm/glm.hpp>

namespace UI {
class GenericElement;

class Button : public Scalable {
public:
    Panel *bgPanel;
    Label *fgLabel;
    
    ~Button();

    /* the Button object will own the panel and label, and adjust them to fit each other. */
    Button(glm::vec2 position, glm::vec2 scale, Panel *panel, Label *label);
};
}