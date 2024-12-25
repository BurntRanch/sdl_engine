#include "object.hpp"
#include "util.hpp"
#include <assimp/Importer.hpp>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>

Object::~Object() {
    
}

Object::Object(glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
    HighestObjectID++;
    SetObjectID(HighestObjectID);

    SetPosition(position);
    SetRotation(rotation);
    SetScale(scale);
}

void Object::ImportFromFile(const std::string &path) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ForceGenNormals | /*aiProcess_GenSmoothNormals |*/ aiProcess_FlipUVs/* | aiProcess_CalcTangentSpace*/);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error(fmt::format("Couldn't load models from assimp: {}", importer.GetErrorString()));
    }

    m_SourceFile = path;
    m_GeneratedFromFile = true;

    ProcessNode(scene->mRootNode, scene);
}

std::string Object::GetSourceFile() {
    return m_SourceFile;
}

void Object::SetIsGeneratedFromFile(bool isGeneratedFromFile) {
    m_GeneratedFromFile = isGeneratedFromFile;
}

bool Object::IsGeneratedFromFile() {
    return m_GeneratedFromFile;
}

/* if parent is nullptr, that must mean this is the rootNode. */
void Object::ProcessNode(aiNode *node, const aiScene *scene, Object *parent) {
    fmt::println("Processing node!");

    Object *obj = this;
    
    if (node != scene->mRootNode) {
        obj = new Object();

        obj->SetParent(parent);

        obj->SetIsGeneratedFromFile(true);

        fmt::println("Node {} is a child to {} object.", fmt::ptr(node), fmt::ptr(parent));
    }

    fmt::println("Node {} is represented by Object {}.", fmt::ptr(node), fmt::ptr(obj));

    aiVector3D position;
    aiQuaternion rotation;
    aiVector3D scale;

    node->mTransformation.Decompose(scale, rotation, position);

    obj->SetPosition(glm::vec3(position.x, position.y, position.z));
    obj->SetRotation(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    obj->SetScale(glm::vec3(scale.x, scale.y, scale.z));

    if (node->mNumMeshes > 0) {
        Model *model = new Model();

        fmt::println("Object {} has atleast 1 mesh!", fmt::ptr(obj));

        for (Uint32 i = 0; i < node->mNumMeshes; i++) {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

            model->meshes.push_back(model->processMesh(mesh, scene));
        }

        obj->AddModelAttachment(model);
    }

    for (Uint32 i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, obj);
    }
}

void Object::AddModelAttachment(Model *model) {
    UTILASSERT(model->GetObjectAttachment() == nullptr);

    m_ModelAttachments.push_back(model);
    model->SetObjectAttachment(this);
}

std::vector<Model *> Object::GetModelAttachments() {
    return m_ModelAttachments;
}

void Object::RemoveModelAttachment(Model *model) {
    auto modelIter = std::find(m_ModelAttachments.begin(), m_ModelAttachments.end(), model);

    if (modelIter != m_ModelAttachments.end()) {
        model->SetObjectAttachment(nullptr);
        m_ModelAttachments.erase(modelIter);
    }

    return;
}

void Object::SetParent(Object *parent) {
    if (parent == m_Parent) {
        return;
    }

    if (m_Parent != nullptr) {
        /* This sounds like an infinite call, but RemoveChild searches for the child, so by this call it won't exist in m_Children anymore and it would just return early. */
        m_Parent->RemoveChild(this);
    }

    m_Parent = parent;

    if (parent != nullptr) {
        /* Same for the other comment above. */
        parent->AddChild(this);
    }
}

Object *Object::GetParent() {
    return m_Parent;
}

void Object::AddChild(Object *child) {
    if (std::find(m_Children.begin(), m_Children.end(), child) != m_Children.end()) {
        return;
    }

    m_Children.push_back(child);

    child->SetParent(this);
}

std::vector<Object *> Object::GetChildren() {
    return m_Children;
}

void Object::RemoveChild(Object *child) {
    auto childIter = std::find(m_Children.begin(), m_Children.end(), child);

    if (childIter != m_Children.end()) {
        m_Children.erase(childIter);
        child->SetParent(nullptr);
    }

    return;
}

void Object::SetCameraAttachment(Camera *camera) {
    /* TODO: same as AddModelAttachment, also consider making it only 1 model attachment per object. */

    m_CameraAttachment = camera;
}

Camera *Object::GetCameraAttachment() {
    return m_CameraAttachment;
}

void Object::SetPosition(glm::vec3 position) {
    m_Position = position;
}

void Object::SetRotation(glm::quat rotation) {
    m_Rotation = rotation;
}

void Object::SetScale(glm::vec3 scale) {
    m_Scale = scale;
}

glm::vec3 Object::GetPosition() {
    return m_Position + (m_Parent != nullptr ? m_Parent->GetPosition() : glm::vec3(0));
}

glm::quat Object::GetRotation() {
    return m_Rotation * (m_Parent != nullptr ? m_Parent->GetRotation() : glm::quat(0, 0, 0, 1));
}

glm::vec3 Object::GetScale() {
    return m_Scale * (m_Parent != nullptr ? m_Parent->GetScale() : glm::vec3(1));
}

int Object::GetObjectID() {
    return m_ObjectID;
}

void Object::SetObjectID(int objectID) {
    m_ObjectID = objectID;
}