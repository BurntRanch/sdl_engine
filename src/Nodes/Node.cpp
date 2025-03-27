#include "Node/Node.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"

#include "util.hpp"
#include <SDL3/SDL_stdinc.h>
#include <assimp/Importer.hpp>
#include <assimp/metadata.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <functional>
#include <glm/trigonometric.hpp>
#include <memory>

Node::~Node() {
    for (Node *obj : m_Children) {
        delete obj;
    }

    m_SceneTree->UnloadNode(this);
}

Node::Node() {
    HighestNodeID++;
    SetNodeID(HighestNodeID);
}

void Node::SetParent(Node *parent) {
    if (parent == m_Parent) {
        return;
    }

    if (m_Parent != nullptr) {
        /* This sounds like an infinite call, but RemoveChild searches for the child, so by this call it won't exist in m_Children anymore and it would just return early. */
        m_Parent->RemoveChild(this);
    }
    /* This node was orphaned, but is now being added to a tree. */
    if (parent != nullptr && m_SceneTree != parent->m_SceneTree) {
        if (m_SceneTree != nullptr) {
            m_SceneTree->UnloadNode(this);
        }
        if (parent->m_SceneTree != nullptr) {
            m_SceneTree = parent->m_SceneTree;
            m_SceneTree->LoadNode(this);
        }
    }

    m_Parent = parent;

    if (parent != nullptr) {
        /* Same for the other comment above. */
        parent->AddChild(this);
    }

    if (m_Parent == nullptr && m_SceneTree != nullptr) {
        m_SceneTree->UnloadNode(this);
    }
}

const Node *Node::GetParent() const {
    return m_Parent;
}

void Node::AddChild(Node *child) {
    if (std::find(m_Children.begin(), m_Children.end(), child) != m_Children.end()) {
        return;
    }

    m_Children.push_back(child);

    child->SetParent(this);
}

const std::vector<Node *> &Node::GetChildren() const {
    return m_Children;
}

void Node::RemoveChild(Node *child) {
    auto childIter = std::find(m_Children.begin(), m_Children.end(), child);

    if (childIter != m_Children.end()) {
        m_Children.erase(childIter);
        child->SetParent(nullptr);
    }

    return;
}

const int &Node::GetNodeID() const {
    return m_NodeID;
}

void Node::SetNodeID(const int nodeID) {
    m_NodeID = nodeID;
}

int Node::HighestNodeID = -1;
