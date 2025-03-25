#ifndef _NODE3D_HPP_
#define _NODE3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "model.hpp"
#include <functional>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <Node/Node.hpp>

class Node3D : public Node {
public:
    ~Node3D();

    Node3D();
    Node3D(const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale);

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    // void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    // void ExportglTF2(const std::string &path);

    virtual void SetPosition(glm::vec3 position);
    virtual glm::vec3 GetPosition();

    virtual void SetRotation(glm::quat rotation);
    virtual glm::quat GetRotation();

    virtual void SetScale(glm::vec3 scale);
    virtual glm::vec3 GetScale();
protected:
    void ProcessNode(aiNode *node, const aiScene *scene, int &sourceID, Node *parent = nullptr, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    glm::vec3 m_Position = glm::vec3(0, 0, 0);
    glm::quat m_Rotation = glm::quat(0, 0, 0, 1);
    glm::vec3 m_Scale = glm::vec3(1, 1, 1);
};

#endif // _NODE3D_HPP_