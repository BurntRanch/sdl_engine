#pragma once

#include "camera.hpp"
#include "model.hpp"
#include <glm/gtc/quaternion.hpp>

class Object {
public:
    ~Object();

    Object(glm::vec3 position = glm::vec3(0.0f), glm::quat rotation = glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3 scale = glm::vec3(1.0f));

    /* Loads a model/scene file with assimp, preferrably glTF 2.0 files.
        Nodes are converted to objects and their meshes are converted into a Model attachment. */
    void ImportFromFile(const std::string &path);

    /* Gets the source path if the object had ImportFromFile called on it.
        Returns empty if the object didn't come from a file or is the child of an object that did. */
    std::string GetSourceFile();

    void AddModelAttachment(Model *model);
    std::vector<Model *> GetModelAttachments();
    void RemoveModelAttachment(Model *model);

    void SetCameraAttachment(Camera *camera);
    Camera *GetCameraAttachment();

    void SetPosition(glm::vec3 position);
    void SetRotation(glm::quat rotation);
    void SetScale(glm::vec3 scale);

    glm::vec3 GetPosition();
    glm::quat GetRotation();
    glm::vec3 GetScale();

    void SetParent(Object *parent);
    Object *GetParent();

    void AddChild(Object *child);
    std::vector<Object *> GetChildren();
    void RemoveChild(Object *child);

    /* Sets whether the object was sourced from an ImportFromFile call or not. */
    void SetIsGeneratedFromFile(bool isGeneratedFromFile);

    /* Gets whether the object was sourced from an ImportFromFile call or not. */
    bool IsGeneratedFromFile();

    int GetObjectID();
    void SetObjectID(int objectID);
private:
    void ProcessNode(aiNode *node, const aiScene *scene, Object *parent = nullptr);

    int m_ObjectID = -1;

    /* If the object was generated from a parent object importing, this will be true. */
    bool m_GeneratedFromFile = false;
    std::string m_SourceFile;

    Object *m_Parent = nullptr;

    std::vector<Object *> m_Children;

    std::vector<Model *> m_ModelAttachments;
    Camera *m_CameraAttachment = nullptr;

    glm::vec3 m_Position;
    glm::quat m_Rotation;
    glm::vec3 m_Scale;
};