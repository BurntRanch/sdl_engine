#ifndef MODEL_HPP
#define MODEL_HPP
#include <SDL3/SDL_stdinc.h>
#include <assimp/material.h>
#include <complex>
#include <filesystem>
#include <fmt/core.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

class Mesh {
public:
    // mesh data
    vector<Vertex>       vertices;
    vector<Uint32> indices;
    path                 diffuseMapPath;
    // glm::vec3            ambient;
    // glm::vec3            specular;
    glm::vec3            diffuse;
    // float shininess;
    // float roughness;
    // float metallic;

    Mesh() = default;

    Mesh(vector<Vertex> vertices, vector<Uint32> indices, path diffuseMapPath, /*float shininess = 0.0, float roughness = 0.0, float metallic = 0.0, glm::vec3 ambient = glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3 specular = glm::vec3(1.0f, 1.0f, 1.0f), */glm::vec3 diffuse = glm::vec3(1.0f, 1.0f, 1.0f)) {
        this->vertices = vertices;
        this->indices = indices;
        this->diffuseMapPath = diffuseMapPath;
        // this->ambient = ambient;
        // this->specular = specular;
        this->diffuse = diffuse;
        // this->shininess = shininess;
        // this->roughness = roughness;
        // this->metallic = metallic;
    };
};

class Model 
{
public:
    vector<Mesh> meshes;

    // [0] = higher
    // [1] = lower
    std::array<glm::vec3, 2> boundingBox;

    Model() = default;

    Model(const string_view path, glm::vec3 position = glm::vec3(0, 0, 0), glm::vec3 rotation = glm::vec3(0, 0, 0));

    constexpr void SetPosition(glm::vec3 pos) { m_Position = pos; m_NeedsUpdate = true; };
    constexpr void SetRotation(glm::vec3 rot) { m_Rotation = rot; m_NeedsUpdate = true; };

    constexpr glm::vec3 GetPosition() { return m_Position; };
    constexpr glm::vec3 GetRotation() { return m_Rotation; };
    glm::mat4 GetModelMatrix();
private:
    // model data
    //vector<Texture> textures_loaded;
    string m_Directory;

    glm::vec3 m_Position;
    glm::vec3 m_Rotation;

    glm::mat4 m_ModelMatrix;
    bool m_NeedsUpdate = false;   // flag, set to true when Position and Rotation are updated, set to false when GetModelMatrix is called.

    void loadModel(string_view path);

    void processNode(aiNode *node, const aiScene *scene);

    Mesh processMesh(aiMesh *mesh, const aiScene *scene);

    //Texture loadDefaultTexture(string typeName);
};

#endif