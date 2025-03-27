#ifndef _NODE_HPP_
#define _NODE_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "SceneTree.hpp"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>

class Node {
public:
    virtual ~Node();

    Node();

    virtual void SetParent(Node *parent);
    virtual const Node *GetParent() const;

    /* Meant to be called by the child when SetParent is called. */
    virtual void AddChild(Node *child);
    
    virtual const std::vector<Node *> &GetChildren() const;
    virtual void RemoveChild(Node *child);

    virtual const int &GetNodeID() const;
    virtual void SetNodeID(const int nodeID);
friend class SceneTree;
protected:
    int m_NodeID = -1;

    Node *m_Parent = nullptr;

    std::vector<Node *> m_Children;

    SceneTree *m_SceneTree = nullptr;

    static int HighestNodeID;
};

#endif // _NODE_HPP_
