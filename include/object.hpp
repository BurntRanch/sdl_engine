#ifndef _OBJECT_HPP_
#define _OBJECT_HPP_

#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "model.hpp"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>

class Object {
public:
    ~Object();

    Object(glm::vec3 position = glm::vec3(0.0f), glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3 scale = glm::vec3(1.0f), int objectID = -1);

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment.
        If there's atleast 1 camera, and if primaryCamOutput is set, it will set primaryCamOutput to the first camera it sees. primaryCamOutput MUST be null!! */
    void ImportFromFile(const std::string &path, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    /* Gets the source path if the object had ImportFromFile called on it.
        Returns empty if the object didn't come from a file or is the child of an object that did. */
    std::string GetSourceFile();

    /* If IsGeneratedFromFile is true, this will be a representation of the node in the 3D model. This can be used to link with a Networking_Object representation where the ObjectIDs might not match. */
    int GetSourceID();
    void SetSourceID(int sourceID);

    void AddModelAttachment(Model *model);
    std::vector<Model *> GetModelAttachments();
    void RemoveModelAttachment(Model *model);

    void SetCameraAttachment(Camera *camera);
    Camera *GetCameraAttachment();

    void SetPosition(glm::vec3 position);
    void SetRotation(glm::quat rotation);
    void SetScale(glm::vec3 scale);

    /* withInheritance controls whether this returns its position in respect to its parent or not. */
    glm::vec3 GetPosition(bool withInheritance = true);
    glm::quat GetRotation(bool withInheritance = true);
    glm::vec3 GetScale(bool withInheritance = true);

    void SetParent(Object *parent);
    Object *GetParent();

    /* Meant to be called by the child when SetParent is called. */
    void AddChild(Object *child);
    
    std::vector<Object *> GetChildren();
    void RemoveChild(Object *child);

    /* Sets whether the object was sourced from an ImportFromFile call or not. */
    void SetIsGeneratedFromFile(bool isGeneratedFromFile);

    /* Gets whether the object was sourced from an ImportFromFile call or not. */
    bool IsGeneratedFromFile();

    void CreateRigidbody(btRigidBody::btRigidBodyConstructionInfo &constructionInfo);
    std::shared_ptr<btRigidBody> &GetRigidBody();
    void DeleteRigidbody();

    int GetObjectID();
    void SetObjectID(int objectID);
private:
    void ProcessNode(aiNode *node, const aiScene *scene, int &sourceID, Object *parent = nullptr, std::optional<std::reference_wrapper<Camera *>> primaryCamOutput = {});

    void SynchronizePhysicsTransform();

    int m_ObjectID = -1;

    /* If the object was generated from a parent object importing, this will be true. */
    bool m_GeneratedFromFile = false;
    std::string m_SourceFile;
    int m_SourceID = 0;

    Object *m_Parent = nullptr;

    std::vector<Object *> m_Children;

    std::vector<Model *> m_ModelAttachments;
    Camera *m_CameraAttachment = nullptr;

    glm::vec3 m_Position;
    glm::quat m_Rotation;
    glm::vec3 m_Scale;

    std::shared_ptr<btRigidBody> m_RigidBody;

    static int HighestObjectID;
};

#endif // _OBJECT_HPP_
