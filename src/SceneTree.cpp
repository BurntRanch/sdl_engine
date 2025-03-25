#include "Node/Node.hpp"
#include "Node/Node3D/Camera3D/Camera3D.hpp"
#include "Node/Node3D/Light3D/PointLight3D/PointLight3D.hpp"
#include "Node/Node3D/Model3D/Model3D.hpp"
#include "Node/Node3D/Node3D.hpp"
#include "fmt/format.h"
#include "util.hpp"
#include <assimp/camera.h>
#include <assimp/light.h>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <cstring>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <SceneTree.hpp>
#include <stdexcept>
#include <type_traits>

Node *SceneTree::ProcessNode(const aiNode *aiNode, const aiScene *aiScene) {
    Node *node = new Node();
    Node3D *node3D = nullptr;

    std::string aiNodeName = aiNode->mName.C_Str();

    /* heuristics to convert glTF 2.0 to our scene format */
    if (!aiNode->mTransformation.IsIdentity()) {
        node = dynamic_cast<Node3D *>(node);

        aiVector3D position;
        aiVector3D rotation;
        aiVector3D scale;

        aiNode->mTransformation.Decompose(position, rotation, scale);

        node3D->SetPosition(glm::vec3(position.x, position.y, position.z));
        node3D->SetRotation(glm::vec3(rotation.x, rotation.y, rotation.z)); /* converted to quaternion automatically */
        node3D->SetScale(glm::vec3(scale.x, scale.y, scale.z));
    }

    if (node3D != nullptr && aiNode->mNumMeshes > 0) {
        node3D = dynamic_cast<Model3D *>(node3D);
        reinterpret_cast<Model3D *>(node3D)->ImportFromAssimpNode(aiNode, aiScene);
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

    if (node3D != nullptr && aiLightPtr != aiScene->mLights + aiScene->mNumLights) {
        fmt::println("Found a light!");

        if (typeid(node3D) != typeid(Node3D *)) {
            throw std::runtime_error("Non-light node has the same name as a light source? This is most likely an error with the model file.");
        }

        aiLight *aiLight = *aiLightPtr;
        
        UTILASSERT(aiLight->mType == aiLightSource_POINT);

        node3D = dynamic_cast<PointLight3D *>(node3D);
        PointLight3D *pointLight = reinterpret_cast<PointLight3D *>(node3D);

        pointLight->SetAttenuation(aiLight->mAttenuationConstant, aiLight->mAttenuationLinear, aiLight->mAttenuationQuadratic);
        pointLight->SetLightColor(glm::vec3(aiLight->mColorDiffuse.r, aiLight->mColorDiffuse.g, aiLight->mColorDiffuse.b));
    }

    if (node3D != nullptr && aiCameraPtr != aiScene->mCameras + aiScene->mNumCameras) {
        fmt::println("Found a camera!");

        if (typeid(node3D) != typeid(Node3D *)) {
            throw std::runtime_error("Non-camera node has the same name as a camera? This is most likely an error with the model file.");
        }

        aiCamera *aiCamera = *aiCameraPtr;

        node3D = dynamic_cast<Camera3D *>(node3D);
        Camera3D *camera = reinterpret_cast<Camera3D *>(node3D);

        glm::vec3 RotationToEulerAngles = glm::eulerAngles(camera->GetRotation());

        camera->SetPitch(RotationToEulerAngles.x);
        camera->SetYaw(RotationToEulerAngles.y);
        camera->SetRoll(RotationToEulerAngles.z);

        camera->SetNear(aiCamera->mClipPlaneNear);
        camera->SetFar(aiCamera->mClipPlaneFar);
        camera->SetFOV(glm::degrees(aiCamera->mHorizontalFOV));
    }

    for (size_t i = 0; i < aiNode->mNumChildren; i++) {
        Node *child = ProcessNode(aiNode->mChildren[0], aiScene);

        node->AddChild(child);
    }

    LoadNode(node);

    return (node3D != nullptr ? node3D : node);
}

void SceneTree::RegisterUnloadListener(const SceneTreeListenerType &func) {
    m_UnloadListeners.push_back(func);
}

void SceneTree::UnloadNode(Node *node) const {
    for (auto &listener : m_UnloadListeners) {
        listener(node, this);
    }

    if (node->GetParent() != nullptr) {
        node->SetParent(nullptr);
    }
}

void SceneTree::LoadNode(Node *node) {
    for (auto &listener : m_LoadListeners) {
        listener(node, this);

        for (Node *child : node->GetChildren()) {
            listener(child, this);
        }
    }

    node->m_SceneTree = this;
}

const Node *SceneTree::GetRootNode() const {
    return m_RootNode;
}

void SceneTree::ImportFromGLTF2(const std::string &path) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ForceGenNormals | /*aiProcess_GenSmoothNormals |*/ aiProcess_FlipUVs/* | aiProcess_CalcTangentSpace*/);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error(fmt::format("Couldn't load models from assimp: {}", importer.GetErrorString()));
    }

    m_RootNode = ProcessNode(scene->mRootNode, scene);
}
