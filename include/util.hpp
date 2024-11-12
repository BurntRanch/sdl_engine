#ifndef UTIL_HPP
#define UTIL_HPP
#include "common.hpp"
#include <fmt/core.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/vec3.hpp>
#include <array>
#include <vector>
#include <string>

#define UTILASSERT(expr)        (static_cast<bool>(expr)    \
                                    ? void(0)   \
                                    : throw std::runtime_error((std::string)#expr + (std::string)" is false/null!"))

bool                     intersects(const glm::vec3 &origin, const glm::vec3 &front, const std::array<glm::vec3, 2> &boundingBox);
std::vector<std::string> split(const std::string_view text, const char delim);
bool                     endsWith(const std::string_view fullString, const std::string_view ending);
glm::vec2                adjustScaleToFitType(UI::Scalable *self, glm::vec2 scale, UI::FitType fitType = UI::UNSET);

#endif
