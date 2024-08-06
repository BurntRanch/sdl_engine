#ifndef UTIL_HPP
#define UTIL_HPP
#include "camera.hpp"
#include <fmt/core.h>
#include <glm/vec3.hpp>
#include <array>
#include <sstream>
#define NULLASSERT(expr)        (static_cast<bool>(expr)    \
                                    ? void(0)   \
                                    : throw std::runtime_error((std::string)#expr + (std::string)" is null!"))

static bool intersects(glm::vec3 origin, glm::vec3 front, std::array<glm::vec3, 2> boundingBox) {
    glm::vec3 inverse_front = 1.0f / front;

    glm::vec3 &box_max = boundingBox[0];
    glm::vec3 &box_min = boundingBox[1];

    float t_near = CAMERA_NEAR;
    float t_far = CAMERA_FAR;

    float t1 = (box_min.x - origin.x) * inverse_front.x;
    float t2 = (box_max.x - origin.x) * inverse_front.x;

    t_near = std::max(t_near, glm::min(t1, t2));
    t_far = std::min(t_far, glm::max(t1, t2));

    float t3 = (box_min.y - origin.y) * inverse_front.y;
    float t4 = (box_max.y - origin.y) * inverse_front.y;

    t_near = glm::max(t_near, glm::min(t3, t4));
    t_far = glm::min(t_far, glm::max(t3, t4));

    return t_near <= t_far && t_far >= 0;
}

static std::vector<std::string> split(std::string_view text, char delim) {
    std::string                 line;
    std::vector<std::string>    vec;
    std::stringstream ss(text.data());
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}

static bool endsWith(std::string_view fullString, std::string_view ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

#endif