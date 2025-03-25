#ifndef _MODEL3D_HPP_
#define _MODEL3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "model.hpp"
#include <assimp/scene.h>
#include <functional>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <Node/Node3D/Node3D.hpp>
#include <material.hpp>

class Model3D : public Node3D {
public:
    ~Model3D();

    Model3D(const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) : Node3D(position, rotation, scale) {};

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    // void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    /* Loads the meshes from an Assimp node.
     * THE NODE MUST CONTAIN ATLEAST ONE MESH. std::runtime_error IF NOT.
     */
    void ImportFromAssimpNode(const aiNode *node, const aiScene *scene);

    Model *GetModel();

    // void ExportglTF2(const std::string &path);

    void SetMaterial(Material *mat);
    Material *GetMaterial();
protected:
    /* Placeholder as we slowly move away from this old Model class. */
    Model *m_Model = nullptr;

    Material *m_Material = nullptr;
};

#endif // _MODEL3D_HPP_