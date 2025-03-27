#ifndef _MODEL3D_HPP_
#define _MODEL3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"

#include <SDL3/SDL_stdinc.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <functional>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <Node/Node3D/Node3D.hpp>
#include <material.hpp>

class Vertex;

class Mesh3D {
public:
    Mesh3D() = default;

    Mesh3D(const std::vector<Vertex>& vertices, const std::vector<Uint32>& indices, const Material &material) {
        m_Vertices = vertices;
        m_Indices = indices;
        m_Material = material;
    };

    void SetMaterial(const Material &material);
    const Material &GetMaterial() const;

    const std::vector<Vertex> &GetVertices() const;
    const std::vector<Uint32> &GetIndices() const;
private:
    // mesh data
    std::vector<Vertex>  m_Vertices;
    std::vector<Uint32>  m_Indices;
    Material m_Material;
};

class Model3D : public Node3D {
public:
    Model3D(const Node &node) : Node3D(node) {};
    Model3D(const Node3D &node3D) : Node3D(node3D) {};
    Model3D(const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) : Node3D(position, rotation, scale) {};

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    // void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    /* Loads the meshes from an Assimp node.
     * THE NODE MUST CONTAIN ATLEAST ONE MESH. std::runtime_error IF NOT.
     */
    void ImportFromAssimpNode(const aiNode *node, const aiScene *scene);

    const std::vector<Mesh3D> &GetMeshes() const;

    const glm::mat4 GetModelMatrix() const;

    // void ExportglTF2(const std::string &path);
protected:
    std::vector<Mesh3D> m_Meshes;

private:
    void ProcessAndAddMesh(const aiMesh *mesh, const aiScene *scene);
};

#endif // _MODEL3D_HPP_