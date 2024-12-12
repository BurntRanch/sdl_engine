#include "object.hpp"

Object::~Object() {
    
}

Object::Object(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) {
    SetPosition(position);
    SetRotation(rotation);
    SetScale(scale);
}

void Object::AddModelAttachment(Model *model) {
    m_ModelAttachments.push_back(model);
}

std::vector<Model *> Object::GetModelAttachments() {
    return m_ModelAttachments;
}

void Object::RemoveModelAttachment(Model *model) {
    auto modelIter = std::find(m_ModelAttachments.begin(), m_ModelAttachments.end(), model);

    if (modelIter != m_ModelAttachments.end()) {
        m_ModelAttachments.erase(modelIter);
    }

    return;
}

void Object::SetPosition(glm::vec3 position) {
    for (Model *&model : m_ModelAttachments) {
        model->SetPosition(model->GetRawPosition() + (position - m_Position));
    }

    m_Position = position;
}

void Object::SetRotation(glm::vec3 rotation) {
    for (Model *&model : m_ModelAttachments) {
        model->SetRotation(model->GetRawRotation() + (rotation - m_Rotation));
    }

    m_Rotation = rotation;
}

void Object::SetScale(glm::vec3 scale) {
    for (Model *&model : m_ModelAttachments) {
        model->SetScale(model->GetRawScale() + (scale - m_Scale));
    }

    m_Scale = scale;
}

glm::vec3 Object::GetPosition() {
    return m_Position;
}

glm::vec3 Object::GetRotation() {
    return m_Rotation;
}

glm::vec3 Object::GetScale() {
    return m_Scale;
}

int Object::GetObjectID() {
    return m_ObjectID;
}

void Object::SetObjectID(int objectID) {
    m_ObjectID = objectID;
}