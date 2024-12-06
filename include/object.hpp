#pragma once

#include "model.hpp"
class Object {
public:
    ~Object() = default;

    Object(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);

    void AddModelAttachment(Model *model);
    std::vector<Model *> GetModelAttachments();
    void RemoveModelAttachment(Model *model);

    void SetPosition(glm::vec3 position);
    void SetRotation(glm::vec3 rotation);
    void SetScale(glm::vec3 scale);

    glm::vec3 GetPosition();
    glm::vec3 GetRotation();
    glm::vec3 GetScale();

    int GetObjectID();
    void SetObjectID(int objectID);
private:
    int m_ObjectID = -1;

    std::vector<Model *> m_ModelAttachments;

    glm::vec3 m_Position;
    glm::vec3 m_Rotation;
    glm::vec3 m_Scale;
};