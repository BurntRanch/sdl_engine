#include "Node/Node.hpp"
#include "fmt/format.h"
#include <assimp/scene.h>
#include <node_util.hpp>

namespace NodeUtil {
    /* TODO: Implement */
    Node *ProcessNode(const aiNode *node, const aiScene *scene) {
        /* :( */
        return nullptr;
    }

    Node *ImportFromGLTF2(const std::string &path) {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ForceGenNormals | /*aiProcess_GenSmoothNormals |*/ aiProcess_FlipUVs/* | aiProcess_CalcTangentSpace*/);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            throw std::runtime_error(fmt::format("Couldn't load models from assimp: {}", importer.GetErrorString()));
        }

        return ProcessNode(scene->mRootNode, scene);
    }

}
