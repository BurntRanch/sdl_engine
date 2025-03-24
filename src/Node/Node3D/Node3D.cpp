#include "Node/Node3D/Node3D.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"
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

Node3D::~Node3D() {
}

Node3D::Node3D() {
    m_Position = glm::vec3(0, 0, 0);
    m_Rotation = glm::quat(0, 0, 0, 1);
    m_Scale = glm::vec3(1, 1, 1);
}

void Node3D::SetPosition(glm::vec3 position) {
    m_Position = position;
}
glm::vec3 Node3D::GetPosition() {
    return m_Position;
}

void Node3D::SetRotation(glm::quat rotation) {
    m_Rotation = rotation;
}
glm::quat Node3D::GetRotation() {
    return m_Rotation;
}

void Node3D::SetScale(glm::vec3 scale) {
    m_Scale = scale;
}
glm::vec3 Node3D::GetScale() {
    return m_Scale;
}
