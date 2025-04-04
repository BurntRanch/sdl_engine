#pragma once

#include <assimp/scene.h>
#include <functional>
#include <string>
#include <vector>

class SceneTree;
class Node;
class Camera3D;
class PointLight3D;

typedef std::function<void(const Node *, const SceneTree *)> SceneTreeListenerType;

/* SceneTrees do not free any unloaded nodes. */
class SceneTree {
public:
    void ImportFromGLTF2(const std::string &path);

    void RegisterUnloadListener(const SceneTreeListenerType &func);
    void RegisterLoadListener(const SceneTreeListenerType &func);

    /* Call any and all listeners so that they are aware of an unload. */
    void UnloadNode(Node *node);
    void LoadNode(Node *node);

    const Node *GetRootNode() const;

    Camera3D *GetMainCamera3D() const;
    const std::vector<PointLight3D *> &GetPointLight3Ds() const;
private:
    Node *ProcessNode(const aiNode *aiNode, const aiScene *aiScene);

    std::vector<SceneTreeListenerType> m_UnloadListeners;
    std::vector<SceneTreeListenerType> m_LoadListeners;

    Node *m_RootNode;

    std::vector<Camera3D *> m_Camera3Ds;

    std::vector<PointLight3D *> m_PointLights3D;

    // std::vector<Camera2D *> m_Camera2Ds;
};
