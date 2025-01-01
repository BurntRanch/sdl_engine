#include "object.hpp"
#include "camera.hpp"
#include "util.hpp"
#include <assimp/Importer.hpp>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <functional>
#include <glm/trigonometric.hpp>

Object::~Object() {
    
}

Object::Object(glm::vec3 position, glm::quat rotation, glm::vec3 scale, int objectID) {
    if (objectID == -1) {
        HighestObjectID++;
        SetObjectID(HighestObjectID);
    } else {
        SetObjectID(objectID);

        if (objectID > HighestObjectID) {
            HighestObjectID = objectID;
        }
    }

    SetPosition(position);
    SetRotation(rotation);
    SetScale(scale);
}

void Object::ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.data(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ForceGenNormals | /*aiProcess_GenSmoothNormals |*/ aiProcess_FlipUVs/* | aiProcess_CalcTangentSpace*/);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error(fmt::format("Couldn't load models from assimp: {}", importer.GetErrorString()));
    }

    m_SourceFile = path;
    m_GeneratedFromFile = true;
    m_SourceID = 0;

    int sourceID = 0;

    ProcessNode(scene->mRootNode, scene, sourceID, nullptr, primaryCamOutput);
}

std::string Object::GetSourceFile() {
    return m_SourceFile;
}

int Object::GetSourceID() {
    return m_SourceID;
}

void Object::SetSourceID(int sourceID) {
    m_SourceID = sourceID;
}

void Object::SetIsGeneratedFromFile(bool isGeneratedFromFile) {
    m_GeneratedFromFile = isGeneratedFromFile;
}

bool Object::IsGeneratedFromFile() {
    return m_GeneratedFromFile;
}

/* if parent is nullptr, that must mean this is the rootNode. */
void Object::ProcessNode(aiNode *node, const aiScene *scene, int &sourceID, Object *parent, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput) {
    fmt::println("Processing node!");

    Object *obj = this;
    
    if (node != scene->mRootNode) {
        obj = new Object();

        obj->SetParent(parent);

        obj->SetIsGeneratedFromFile(true);

        obj->SetSourceID(sourceID);

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

    /* Check if this node is/has a camera. */
    for (Uint32 i = 0; i < scene->mNumCameras; i++) {
        /* Found the camera! */
        if (scene->mCameras[i]->mName == node->mName) {
            aiCamera *sceneCam = scene->mCameras[i];

            aiVector3D direction = aiVector3D(-node->mTransformation.a3, -node->mTransformation.b3, -node->mTransformation.c3);
            direction.Normalize();

            float pitch = glm::degrees(std::asin(-direction.z));
            float yaw = glm::degrees(std::atan2(direction.x, direction.y));

            fmt::println("{} {}", pitch, yaw);

            Camera *cam = new Camera(glm::vec3(sceneCam->mUp.x, sceneCam->mUp.z, sceneCam->mUp.y), -90.0f, -90.0f);

            cam->FOV = glm::degrees(sceneCam->mHorizontalFOV);

            obj->SetCameraAttachment(cam);

            if (primaryCamOutput.has_value() && primaryCamOutput.value().get() == nullptr) {
                primaryCamOutput.value().get() = cam;
            }

            break;
        }
    }

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
        sourceID++;
        ProcessNode(node->mChildren[i], scene, sourceID, obj, primaryCamOutput);
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
    if (m_CameraAttachment == camera) {
        return;
    }

    Camera *oldCameraAttachment = m_CameraAttachment;

    m_CameraAttachment = camera;

    if (oldCameraAttachment != nullptr) {
        oldCameraAttachment->SetObjectAttachment(nullptr);
    }

    if (camera != nullptr) {
        camera->SetObjectAttachment(this);
    }
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

int Object::HighestObjectID = -1;