#ifndef _MODEL3D_HPP_
#define _MODEL3D_HPP_

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
    ~Light3D();

    Light3D();
    Light3D(glm::vec3 position, glm::quat rotation, glm::vec3 scale);

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    // void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    // void ExportglTF2(const std::string &path);

    void SetLightColor(glm::vec3 color);
    glm::vec3 GetLightColor();
private:
    glm::vec3 m_LightColor;
};

#endif // _MODEL3D_HPP_