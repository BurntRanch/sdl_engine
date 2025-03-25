#ifndef _NODE_HPP_
#define _NODE_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "SceneTree.hpp"
#include "camera.hpp"
#include "model.hpp"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>

class Node {
public:
    ~Node();

    Node();

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    // void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    // void ExportglTF2(const std::string &path);

    virtual void SetParent(Node *parent);
    virtual Node *GetParent() const;

    /* Meant to be called by the child when SetParent is called. */
    virtual void AddChild(Node *child);
    
    virtual std::vector<Node *> GetChildren() const;
    virtual void RemoveChild(Node *child);

    virtual int GetNodeID() const;
    virtual void SetNodeID(const int nodeID);
friend class SceneTree;
protected:
    void ProcessNode(aiNode *node, const aiScene *scene, int &sourceID, Node *parent = nullptr, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    int m_NodeID = -1;

    Node *m_Parent = nullptr;

    std::vector<Node *> m_Children;

    SceneTree *m_SceneTree;

    static int HighestNodeID;
};

#endif // _NODE_HPP_
