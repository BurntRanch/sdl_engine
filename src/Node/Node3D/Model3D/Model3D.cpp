#include "Node/Node3D/Model3D/Model3D.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"
#include "camera.hpp"
#include "model.hpp"
#include "util.hpp"
#include <SDL3/SDL_stdinc.h>
#include <assimp/Importer.hpp>
#include <assimp/metadata.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <functional>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <memory>

Model3D::~Model3D() {
    if (m_Model != nullptr)
        delete m_Model;
}

void Model3D::ImportFromAssimpNode(const aiNode *node, const aiScene *scene) {
    UTILASSERT(node->mNumMeshes > 0);

    if (m_Model != nullptr)
        delete m_Model;

    m_Model = new Model();

    for (Uint32 i = 0; i < node->mNumMeshes; ++i) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        m_Model->meshes.push_back(m_Model->processMesh(mesh, scene));
    }
}

Model *Model3D::GetModel() {
    return m_Model;
}

void Model3D::SetMaterial(Material *mat) {
    m_Material = mat;
}

Material *Model3D::GetMaterial() {
    return m_Material;
}
