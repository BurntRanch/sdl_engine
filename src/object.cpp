#include "object.hpp"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btDefaultMotionState.h"
#include "LinearMath/btMotionState.h"
#include "LinearMath/btQuaternion.h"
#include "LinearMath/btTransform.h"
#include "LinearMath/btVector3.h"
#include "camera.hpp"
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

Object::~Object() {
    for (Object *obj : m_Children) {
        delete obj;
    }

    if (m_CameraAttachment) {
        delete m_CameraAttachment;
    }

    for (Model *model : m_ModelAttachments) {
        delete model;
    }
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

void Object::CreateRigidbody(btRigidBody::btRigidBodyConstructionInfo &constructionInfo) {
    m_RigidBody = std::make_shared<btRigidBody>(constructionInfo);

    SynchronizePhysicsTransform();

    m_RigidBody->setUserPointer(this);
}

std::shared_ptr<btRigidBody> &Object::GetRigidBody() {
    return m_RigidBody;
}

void Object::DeleteRigidbody() {
    m_RigidBody.reset();
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

        fmt::println("Node {} (Name: {}) is a child to {} object (SourceID: {}).", fmt::ptr(node), node->mName.C_Str(), fmt::ptr(parent), sourceID);
    }

    fmt::println("Node {} is represented by Object {}.", fmt::ptr(node), fmt::ptr(obj));

    aiVector3D position;
    aiQuaternion rotation;
    aiVector3D scale;

    node->mTransformation.Decompose(scale, rotation, position);

    obj->SetPosition(glm::vec3(position.x, position.z, position.y));
    obj->SetRotation(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));
    obj->SetScale(glm::vec3(scale.x, scale.y, scale.z));

    aiMetadata extensionsData;

    if (node->mMetaData && node->mMetaData->HasKey("extensions")) {
        node->mMetaData->Get("extensions", extensionsData);
    }

    if (extensionsData.HasKey("KHR_physics_rigid_bodies") && scene->mMetaData && scene->mMetaData->HasKey("extensions")) {
        struct glTFRigidBody rigidBody = getColliderInfoFromNode(node, scene);
        struct glTFColliderInfo &colliderInfo = rigidBody.colliderInfo;

        btVector3 localInertia(0, 0, 0);
        if (rigidBody.mass != 0.0f) {
            /* TODO: looks like there's some more inertia information in `KHR_physics_rigid_bodies/motion`, Consider using that. */
            colliderInfo.shape->calculateLocalInertia(rigidBody.mass, localInertia);    
        }

        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(btVector3(position.x, position.y, position.z));
        transform.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

        btRigidBody::btRigidBodyConstructionInfo cInfo{static_cast<btScalar>(rigidBody.mass), new btDefaultMotionState(transform), colliderInfo.shape, localInertia};
        obj->CreateRigidbody(cInfo);

        obj->GetRigidBody()->setFriction(colliderInfo.physicsMaterial.staticFriction);
        obj->GetRigidBody()->setRollingFriction(colliderInfo.physicsMaterial.dynamicFriction);
        obj->GetRigidBody()->setSpinningFriction(colliderInfo.physicsMaterial.dynamicFriction);
        obj->GetRigidBody()->setRestitution(colliderInfo.physicsMaterial.restitution);
    }

    /* Check if this node is/has a camera. */
    for (Uint32 i = 0; i < scene->mNumCameras; ++i) {
        /* Found the camera! */
        if (scene->mCameras[i]->mName == node->mName) {
            aiCamera *sceneCam = scene->mCameras[i];

            UTILASSERT((sceneCam->mOrthographicWidth > 0 || sceneCam->mHorizontalFOV > 0) && !(sceneCam->mOrthographicWidth > 0 && sceneCam->mHorizontalFOV > 0));

            aiVector3D direction = aiVector3D(-node->mTransformation.a3, -node->mTransformation.b3, -node->mTransformation.c3);
            direction.Normalize();

            float pitch = glm::degrees(std::atan2(direction.x, direction.y));
            float yaw = glm::degrees(std::asin(direction.z));

            Camera *cam = new Camera(sceneCam->mAspect, glm::vec3(sceneCam->mUp.x, sceneCam->mUp.y, sceneCam->mUp.z), yaw, pitch);

            if (sceneCam->mHorizontalFOV > 0) {
                cam->FOV = glm::degrees(sceneCam->mHorizontalFOV);
            } else {
                cam->type = CAMERA_ORTHOGRAPHIC;
                cam->OrthographicWidth = sceneCam->mOrthographicWidth;
            }

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

        for (Uint32 i = 0; i < node->mNumMeshes; ++i) {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

            model->meshes.push_back(model->processMesh(mesh, scene));
        }

        obj->AddModelAttachment(model);
    }

    for (Uint32 i = 0; i < node->mNumChildren; ++i) {
        sourceID++;
        ProcessNode(node->mChildren[i], scene, sourceID, obj, primaryCamOutput);
    }
}

void Object::SynchronizePhysicsTransform() {
    if (!m_RigidBody) {
        return;
    }

    m_RigidBody->getMotionState()->setWorldTransform(btTransform(btQuaternion(m_Rotation.x, m_Rotation.y, m_Rotation.z, m_Rotation.w), btVector3(m_Position.x, m_Position.y, m_Position.z)));
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

    SynchronizePhysicsTransform();
}

void Object::SetRotation(glm::quat rotation) {
    m_Rotation = rotation;

    SynchronizePhysicsTransform();
}

void Object::SetScale(glm::vec3 scale) {
    m_Scale = scale;
}

glm::vec3 Object::GetPosition(bool withInheritance) {
    return m_Position + (m_Parent != nullptr && withInheritance ? m_Parent->GetPosition() : glm::vec3(0));
}

glm::quat Object::GetRotation(bool withInheritance) {
    return m_Rotation * (m_Parent != nullptr && withInheritance ? m_Parent->GetRotation() : glm::quat(0, 0, 0, 1));
}

glm::vec3 Object::GetScale(bool withInheritance) {
    return m_Scale * (m_Parent != nullptr && withInheritance ? m_Parent->GetScale() : glm::vec3(1));
}

int Object::GetObjectID() {
    return m_ObjectID;
}

void Object::SetObjectID(int objectID) {
    m_ObjectID = objectID;
}

int Object::HighestObjectID = -1;
