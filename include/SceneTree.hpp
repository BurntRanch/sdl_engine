#pragma once

#include <assimp/scene.h>
#include <functional>
#include <string>
#include <vector>

class SceneTree;
class Node;

typedef std::function<void(const Node *, const SceneTree *)> SceneTreeListenerType;

/* SceneTrees do not free any unloaded nodes. */
class SceneTree {
public:
    void ImportFromGLTF2(const std::string &path);

    void RegisterUnloadListener(const SceneTreeListenerType &func);
    void RegisterLoadListener(const SceneTreeListenerType &func);

    /* Call any and all listeners so that they are aware of an unload. */
    void UnloadNode(Node *node) const;
    void LoadNode(Node *node);

    const Node *GetRootNode() const;
private:
    Node *ProcessNode(const aiNode *aiNode, const aiScene *aiScene);

    std::vector<SceneTreeListenerType> m_UnloadListeners;
    std::vector<SceneTreeListenerType> m_LoadListeners;

    Node *m_RootNode;
};
