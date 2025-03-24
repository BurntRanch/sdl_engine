#ifndef _UTIL_HPP_
#define _UTIL_HPP_
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "common.hpp"
#include "engine.hpp"
#include "Node/Node.hpp"
#include <SDL3/SDL_stdinc.h>
#include <assimp/scene.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/vec3.hpp>
#include <array>
#include <vector>
#include <string>
#include <rapidxml.hpp>



#define UTILASSERT(expr)        (static_cast<bool>(expr)    \
                                    ? void(0)   \
                                    : throw std::runtime_error((std::string)#expr + (std::string)" is false/null!"))

struct glTFPhysicsMaterial {
    float staticFriction;
    float dynamicFriction;

    float restitution;
};

struct glTFColliderInfo {
    btCollisionShape *shape;

    glTFPhysicsMaterial physicsMaterial;

    /* TODO: I don't see a point in collisionFilter but it could be useful. */
};

struct glTFRigidBody {
    /* If this is 0, then the RigidBody is static. */
    Uint64 mass = 0;

    glTFColliderInfo colliderInfo;
};

bool                     intersects(const glm::vec3 &origin, const glm::vec3 &front, const std::array<glm::vec3, 2> &boundingBox);
std::vector<std::string> split(const std::string_view text, const char delim);

/* This function is recursive. */
Object                  *DeepSearchObjectTree(Object *obj, std::function<bool(Object *)> pred);

/* This function is recursive, int = index in candidates */
std::vector<int> FilterRelatedNetworkingObjects(std::vector<Networking_Object> &candidates, Networking_Object *object);

bool                     endsWith(const std::string_view fullString, const std::string_view ending);
glm::vec2                adjustScaleToFitType(UI::Scalable *self, glm::vec2 scale, UI::FitType fitType = UI::UNSET);
rapidxml::xml_node<char>*getPropertiesNode(rapidxml::xml_node<char> *uiObjectNode);
std::string              getID(rapidxml::xml_node<char> *propertiesNode);
glm::vec3                getColor(rapidxml::xml_node<char> *propertiesNode);
glm::vec2                getPosition(rapidxml::xml_node<char> *propertiesNode);
glm::vec2                getScale(rapidxml::xml_node<char> *propertiesNode);
float                    getZDepth(rapidxml::xml_node<char> *propertiesNode, float depthDefault = 1.0f);
bool                     getVisible(rapidxml::xml_node<char> *propertiesNode);

/* Creates a box shape and gives back a pointer that is owned by the caller. */
btCollisionShape *createBoxShape(glm::vec3 size);

struct glTFRigidBody getColliderInfoFromNode(const aiNode *node, const aiScene *scene);

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
