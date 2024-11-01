#include "util.hpp"
#include "camera.hpp"
#include <sstream>

bool endsWith(const std::string_view fullString, const std::string_view ending) {
    if (ending.length() > fullString.length())
        return false;
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
}

std::vector<std::string> split(const std::string_view text, const char delim) {
    std::string                 line;
    std::vector<std::string>    vec;
    std::stringstream ss(text.data());
    while (std::getline(ss, line, delim)) {
        vec.push_back(line);
    }
    return vec;
}

bool intersects(const glm::vec3 &origin, const glm::vec3 &front, const std::array<glm::vec3, 2> &boundingBox) {
    const glm::vec3 inverse_front = 1.0f / front;

    const glm::vec3 &box_max = boundingBox[0];
    const glm::vec3 &box_min = boundingBox[1];

    const float t1 = (box_min.x - origin.x) * inverse_front.x;
    const float t2 = (box_max.x - origin.x) * inverse_front.x;

    float t_near = std::max(CAMERA_NEAR, glm::min(t1, t2));
    float t_far  = std::min(CAMERA_FAR,  glm::max(t1, t2));

    const float t3 = (box_min.y - origin.y) * inverse_front.y;
    const float t4 = (box_max.y - origin.y) * inverse_front.y;

    t_near = glm::max(t_near, glm::min(t3, t4));
    t_far  = glm::min(t_far,  glm::max(t3, t4));

    return t_near <= t_far && t_far >= 0;
}
