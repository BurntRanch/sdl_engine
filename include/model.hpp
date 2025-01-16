#ifndef MODEL_HPP
#define MODEL_HPP

#include "fmt/base.h"

#include <SDL3/SDL_stdinc.h>
#include <assimp/material.h>
#include <cmath>
#include <complex>
#include <filesystem>
#include <glm/ext/vector_float2.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <vulkan/vulkan_core.h>

using std::vector;
using std::string;
using std::string_view;
using std::array;
using namespace std::filesystem;

class Object;

struct Vertex {
    glm::vec3 Position;
    //glm::vec3 Color;  // might add later
    glm::vec3 Normal;
    glm::vec2 TexCoord;
};

struct SimpleVertex {
    glm::vec2 Position;
    //glm::vec3 Color;  // might add later
    glm::vec2 TexCoord;
};

inline struct VkVertexInputBindingDescription getVertexBindingDescription() {
    VkVertexInputBindingDescription bindingDescrption{};
    bindingDescrption.binding = 0;
    bindingDescrption.stride = sizeof(Vertex);
    bindingDescrption.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescrption;
}

inline struct array<VkVertexInputAttributeDescription, 3> getVertexAttributeDescriptions() {
    array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, Position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, Normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, TexCoord);

    return attributeDescriptions;
}

inline struct VkVertexInputBindingDescription getSimpleVertexBindingDescription() {
    VkVertexInputBindingDescription bindingDescrption{};
    bindingDescrption.binding = 0;
    bindingDescrption.stride = sizeof(SimpleVertex);
    bindingDescrption.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescrption;
}

inline struct array<VkVertexInputAttributeDescription, 2> getSimpleVertexAttributeDescriptions() {
    array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(SimpleVertex, Position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(SimpleVertex, TexCoord);

    return attributeDescriptions;
}

class Mesh;

class Model 
{
public:
    vector<Mesh> meshes;

    Model() = default;

    Mesh processMesh(aiMesh *mesh, const aiScene *scene);

    /* Return the models Bounding Box, also transforms the bounding box with the Model Matrix. */
    std::array<glm::vec3, 2> GetBoundingBox() { return {glm::vec4(m_BoundingBox[0], 0.0f) * GetModelMatrix(), glm::vec4(m_BoundingBox[1], 0.0f) * GetModelMatrix()}; };

    /* Return the models Bounding Box, with no transformations, This should not be used for ray checks and such. */
    constexpr std::array<glm::vec3, 2> GetRawBoundingBox() { return m_BoundingBox; };

    constexpr void SetBoundingBox(std::array<glm::vec3, 2> boundingBox) { m_BoundingBox = boundingBox; };

    /* Only 1 object can be attached at a time. */
    void SetObjectAttachment(Object *object);
    Object *GetObjectAttachment();

    glm::mat4 GetModelMatrix();

    int GetModelID();
    void SetModelID(int modelID);
private:
    int m_ModelID = -1;

    Object *m_ObjectAttachment = nullptr;

    // [0] = higher
    // [1] = lower
    std::array<glm::vec3, 2> m_BoundingBox;

    glm::mat4 m_ModelMatrix = glm::mat4(1.0f);

    //Texture loadDefaultTexture(string typeName);
};


class Mesh {
public:
    // mesh data
    vector<Vertex>       vertices;
    vector<Uint32>       indices;
    path                 diffuseMapPath;
    // glm::vec3            ambient;
    // glm::vec3            specular;
    glm::vec3            diffuse;
    // float shininess;
    // float roughness;
    // float metallic;

    Mesh() = default;

    Mesh(Model &parent, vector<Vertex> vertices, vector<Uint32> indices, path diffuseMapPath, /*float shininess = 0.0, float roughness = 0.0, float metallic = 0.0, glm::vec3 ambient = glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f), */glm::vec3 diffuse = glm::vec3(1.0f, 1.0f, 1.0f)) : m_Parent(&parent) {
        this->vertices = vertices;
        this->indices = indices;
        this->diffuseMapPath = diffuseMapPath;
        // this->ambient = ambient;
        // this->specular = specular;
        this->diffuse = diffuse;
        // this->shininess = shininess;
        // this->roughness = roughness;
        // this->metallic = metallic;

        for (Vertex &vertex : vertices) {
            m_BoundingBox[0].x = glm::max(vertex.Position.x, m_BoundingBox[0].x);
            m_BoundingBox[1].x = glm::min(vertex.Position.x, m_BoundingBox[1].x);

            m_BoundingBox[0].y = glm::max(vertex.Position.y, m_BoundingBox[0].y);
            m_BoundingBox[1].y = glm::min(vertex.Position.y, m_BoundingBox[1].y);

            m_BoundingBox[0].z = glm::max(vertex.Position.z, m_BoundingBox[0].z);
            m_BoundingBox[1].z = glm::min(vertex.Position.z, m_BoundingBox[1].z);
        }
    };

    std::array<glm::vec3, 2> GetBoundingBox() {
        if (!m_Parent) {
            throw std::runtime_error("Tried to get the bounding box of an orphaned Mesh! (a Model parent is required for this)");
        }

        return {glm::vec4(m_BoundingBox[0], 0.0f) * m_Parent->GetModelMatrix(), glm::vec4(m_BoundingBox[1], 0.0f) * m_Parent->GetModelMatrix()};
    }
private:
    Model *m_Parent = nullptr;

    // [0] = higher
    // [1] = lower
    std::array<glm::vec3, 2> m_BoundingBox = {glm::vec3(-INFINITY), glm::vec3(INFINITY)};
};

#endif
