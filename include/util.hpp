#ifndef UTIL_HPP
#define UTIL_HPP
#include "common.hpp"
#include "engine.hpp"
#include "fmt/base.h"
#include "isteamnetworkingsockets.h"
#include "object.hpp"
#include "steamnetworkingtypes.h"
#include <glm/ext/vector_float2.hpp>
#include <glm/vec3.hpp>
#include <array>
#include <unordered_map>
#include <vector>
#include <string>
#include <rapidxml.hpp>



#define UTILASSERT(expr)        (static_cast<bool>(expr)    \
                                    ? void(0)   \
                                    : throw std::runtime_error((std::string)#expr + (std::string)" is false/null!"))

bool                     intersects(const glm::vec3 &origin, const glm::vec3 &front, const std::array<glm::vec3, 2> &boundingBox);
std::vector<std::string> split(const std::string_view text, const char delim);

/* This function is recursive. */
Object                  *DeepSearchObjectTree(Object *obj, std::function<bool(Object *)> pred);

/* This function is recursive, int = index in candidates */
std::vector<std::pair<Networking_Object *, int>> FilterRelatedNetworkingObjects(std::vector<Networking_Object> &candidates, Networking_Object *object);

bool                     endsWith(const std::string_view fullString, const std::string_view ending);
glm::vec2                adjustScaleToFitType(UI::Scalable *self, glm::vec2 scale, UI::FitType fitType = UI::UNSET);
rapidxml::xml_node<char>*getPropertiesNode(rapidxml::xml_node<char> *uiObjectNode);
std::string              getID(rapidxml::xml_node<char> *propertiesNode);
glm::vec3                getColor(rapidxml::xml_node<char> *propertiesNode);
glm::vec2                getPosition(rapidxml::xml_node<char> *propertiesNode);
glm::vec2                getScale(rapidxml::xml_node<char> *propertiesNode);
float                    getZDepth(rapidxml::xml_node<char> *propertiesNode, float depthDefault = 1.0f);
bool                     getVisible(rapidxml::xml_node<char> *propertiesNode);

template<typename T>
void Deserialize(std::vector<std::byte> &object, T &dest) {
    if constexpr (std::is_same<T, std::string>::value) {
        UTILASSERT(object.size() >= sizeof(size_t));    /* minimum size */

        size_t stringSize = *reinterpret_cast<size_t *>(object.data());
        object.erase(object.begin(), object.begin() + sizeof(size_t));

        UTILASSERT(object.size() >= stringSize);    /* Each char is 1 byte, this is valid. */

        char *string = reinterpret_cast<char *>(object.data());

        dest = std::string(string, stringSize);

        object.erase(object.begin(), object.begin() + stringSize);
    } else {
        UTILASSERT(object.size() >= sizeof(T));
        
        dest = *reinterpret_cast<T *>(object.data());

        object.erase(object.begin(), object.begin() + sizeof(T));
    }
}

template<typename T>
void Serialize(T object, std::vector<std::byte> &dest) {
    if constexpr (std::is_same<T, std::string>::value) {
        Serialize(object.size(), dest);

        for (char &c : object) {
            Serialize(c, dest);
        }
    } else {
        for (size_t i = 0; i < sizeof(T); i++) {
            std::byte *byte = reinterpret_cast<std::byte *>(&object) + i;
            dest.push_back(*byte);
        }
    }
}

#endif
