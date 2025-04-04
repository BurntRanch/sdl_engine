#include "Node/Node.hpp"
#include "Node/Node3D/Camera3D/Camera3D.hpp"
#include "Node/Node3D/Light3D/PointLight3D/PointLight3D.hpp"
#include "Node/Node3D/Model3D/Model3D.hpp"
#include "Node/Node3D/Node3D.hpp"
#include "fmt/format.h"
#include "util.hpp"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/camera.h>
#include <assimp/light.h>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <cstring>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <SceneTree.hpp>
#include <stdexcept>
#include <type_traits>

Node *SceneTree::ProcessNode(const aiNode *aiNode, const aiScene *aiScene) {
    Node *node = new Node();

    std::string aiNodeName = aiNode->mName.C_Str();

    /* heuristics to convert glTF 2.0 to our scene format */
    if (!aiNode->mTransformation.IsIdentity()) {
        node = new Node3D(*node);

        aiVector3D position;
        aiVector3D rotation;
        aiVector3D scale;

        aiNode->mTransformation.Decompose(scale, rotation, position);

        reinterpret_cast<Node3D *>(node)->SetPosition(glm::vec3(position.x, position.y, position.z));
        reinterpret_cast<Node3D *>(node)->SetRotation(glm::vec3(rotation.x, rotation.y, rotation.z)); /* converted to quaternion automatically */
        reinterpret_cast<Node3D *>(node)->SetScale(glm::vec3(scale.x, scale.y, scale.z));
    }

    if (typeid(*node) == typeid(Node3D) && aiNode->mNumMeshes > 0) {
        node = new Model3D(*dynamic_cast<Node3D *>(node));
        reinterpret_cast<Model3D *>(node)->ImportFromAssimpNode(aiNode, aiScene);
    }

    /* find a light with the same name according to the assimp documentation. */
    auto aiLightPtr = std::find_if(aiScene->mLights, aiScene->mLights + aiScene->mNumLights, 
        [aiNodeName] (aiLight *light) 
            { 
                return strcmp(light->mName.C_Str(), aiNodeName.c_str()) == 0; 
            }
        );

    /* find a camera with the same name according to the assimp documentation. */
    auto aiCameraPtr = std::find_if(aiScene->mCameras, aiScene->mCameras + aiScene->mNumCameras, 
        [aiNodeName] (aiCamera *camera) 
            { 
                return strcmp(camera->mName.C_Str(), aiNodeName.c_str()) == 0; 
            }
        );

    if (typeid(*node) == typeid(Node3D) && aiLightPtr != aiScene->mLights + aiScene->mNumLights) {
        fmt::println("Found a light!");

        aiLight *aiLight = *aiLightPtr;
        
        UTILASSERT(aiLight->mType == aiLightSource_POINT);

        node = new PointLight3D(*dynamic_cast<Node3D *>(node), aiLight->mAttenuationConstant, aiLight->mAttenuationLinear, aiLight->mAttenuationQuadratic);

        reinterpret_cast<PointLight3D *>(node)->SetLightColor(glm::vec3(aiLight->mColorDiffuse.r, aiLight->mColorDiffuse.g, aiLight->mColorDiffuse.b));
    }

    if (typeid(*node) == typeid(Node3D) && aiCameraPtr != aiScene->mCameras + aiScene->mNumCameras) {
        fmt::println("Found a camera!");

        aiCamera *aiCamera = *aiCameraPtr;

        node = new Camera3D(*dynamic_cast<Node3D *>(node));
        Camera3D *camera = reinterpret_cast<Camera3D *>(node);

        camera->SetNear(aiCamera->mClipPlaneNear);
        camera->SetFar(aiCamera->mClipPlaneFar);
        camera->SetFOV(glm::degrees(aiCamera->mHorizontalFOV));

        camera->SetUp(glm::vec3(aiCamera->mUp.x, aiCamera->mUp.y, aiCamera->mUp.z));
    }

    for (size_t i = 0; i < aiNode->mNumChildren; i++) {
        Node *child = ProcessNode(aiNode->mChildren[i], aiScene);

        node->AddChild(child);
    }

    LoadNode(node);

    return node;
}

void SceneTree::RegisterUnloadListener(const SceneTreeListenerType &func) {
    m_UnloadListeners.push_back(func);
}

void SceneTree::RegisterLoadListener(const SceneTreeListenerType &func) {
    m_LoadListeners.push_back(func);
}

void SceneTree::UnloadNode(Node *node) {
    for (auto &listener : m_UnloadListeners) {
        listener(node, this);
    }

    if (node->GetParent() != nullptr) {
        node->SetParent(nullptr);
    }

    node->m_SceneTree = nullptr;

    auto camera3D_it = std::find(m_Camera3Ds.begin(), m_Camera3Ds.end(), node);
    if (camera3D_it != m_Camera3Ds.end()) {
        m_Camera3Ds.erase(camera3D_it);
    }

    auto pointLight3D_it = std::find(m_PointLights3D.begin(), m_PointLights3D.end(), node);
    if (pointLight3D_it != m_PointLights3D.end()) {
        m_PointLights3D.erase(pointLight3D_it);
    }

    for (Node *child : node->GetChildren()) {
        UnloadNode(child);
    }
}

void SceneTree::LoadNode(Node *node) {
    for (auto &listener : m_LoadListeners) {
        listener(node, this);
    }

    node->m_SceneTree = this;

    if (typeid(*node) == typeid(Camera3D)) {
        m_Camera3Ds.push_back(dynamic_cast<Camera3D *>(node));
    }

    if (typeid(*node) == typeid(PointLight3D)) {
        m_PointLights3D.push_back(dynamic_cast<PointLight3D *>(node));
    }
    
    for (Node *child : node->GetChildren()) {
        LoadNode(child);
    }
}

const Node *SceneTree::GetRootNode() const {
    return m_RootNode;
}

Camera3D *SceneTree::GetMainCamera3D() const {
    if (m_Camera3Ds.empty()) {
        return nullptr;
    }

    return m_Camera3Ds[0];
}

const std::vector<PointLight3D *> &SceneTree::GetPointLight3Ds() const {
    return m_PointLights3D;
}

void SceneTree::ImportFromGLTF2(const std::string &path) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ForceGenNormals | /*aiProcess_GenSmoothNormals |*/ aiProcess_FlipUVs/* | aiProcess_CalcTangentSpace*/);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error(fmt::format("Couldn't load models from assimp: {}", importer.GetErrorString()));
    }

    m_RootNode = ProcessNode(scene->mRootNode, scene);
}
