#ifndef _LIGHT3D_HPP_
#define _LIGHT3D_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "model.hpp"
#include <functional>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <Node/Node3D/Node3D.hpp>
#include <material.hpp>

class Light3D : public Node3D {
public:
    Light3D() = default;

    Light3D(const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) : Node3D(position, rotation, scale) {};

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    // void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    // void ExportglTF2(const std::string &path);

    void SetLightColor(glm::vec3 color);
    glm::vec3 GetLightColor();
protected:
    glm::vec3 m_LightColor = glm::vec3(1, 1, 1);
};

#endif // _LIGHT3D_HPP_