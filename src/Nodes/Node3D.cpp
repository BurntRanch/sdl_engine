#include "Node/Node3D/Node3D.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"
#include "Node/Node.hpp"
#include "camera.hpp"
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

Node3D::Node3D(const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale) {
    m_Position = position;
    m_Rotation = rotation;
    m_Scale = scale;
}

void Node3D::SetPosition(const glm::vec3 position) {
    m_Position = position;
}
const glm::vec3 &Node3D::GetPosition() const {
    return m_Position;
}
const glm::vec3 Node3D::GetAbsolutePosition() const {
    if (Node3D *parentAsNode3D = dynamic_cast<Node3D *>(m_Parent)) {
        return m_Position + parentAsNode3D->GetAbsolutePosition();
    }
    return m_Position;
}

void Node3D::SetRotation(const glm::quat rotation) {
    m_Rotation = rotation;
}
const glm::quat &Node3D::GetRotation() const {
    return m_Rotation;
}
const glm::quat Node3D::GetAbsoluteRotation() const {
    if (Node3D *parentAsNode3D = dynamic_cast<Node3D *>(m_Parent)) {
        return m_Rotation * parentAsNode3D->GetAbsoluteRotation();
    }
    return m_Rotation;
}

void Node3D::SetScale(const glm::vec3 scale) {
    m_Scale = scale;
}
const glm::vec3 &Node3D::GetScale() const {
    return m_Scale;
}
const glm::vec3 Node3D::GetAbsoluteScale() const {
    if (Node3D *parentAsNode3D = dynamic_cast<Node3D *>(m_Parent)) {
        return m_Scale * parentAsNode3D->GetAbsoluteScale();
    }
    return m_Scale;
}
