#include "engine.hpp"

#include "BulletCollision/BroadphaseCollision/btDbvtBroadphase.h"
#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btVector3.h"
#include "camera.hpp"
#include "common.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "isteamnetworkingsockets.h"
#include "networking/connection.hpp"
#include "object.hpp"
#include "renderer/vulkanRenderer.hpp"
#include "steamclientpublic.h"
#include "steamnetworkingsockets.h"
#include "model.hpp"
#include "steamnetworkingtypes.h"
#include "ui/button.hpp"
#include "ui/panel.hpp"
#include "ui.hpp"
#include "util.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <glm/fwd.hpp>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

Engine::Engine() {
    SDL_Init(SDL_INIT_EVENTS);
}

Engine::~Engine() {
    DeinitPhysics();
}

void Engine::InitRenderer(Settings &settings, Camera *primaryCamera) {
    m_Settings = &settings;

    m_Renderer = new VulkanRenderer(settings, primaryCamera);

    m_Renderer->Init();

    RegisterSDLEventListener(std::bind(&Engine::CheckButtonClicks, this, std::placeholders::_1), SDL_EVENT_MOUSE_BUTTON_UP);
}

void Engine::InitPhysics() {
    m_CollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    m_Dispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfig.get());
    m_Broadphase = std::make_unique<btDbvtBroadphase>();
    m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();

    m_DynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(m_Dispatcher.get(), m_Broadphase.get(), m_Solver.get(), m_CollisionConfig.get());

    m_DynamicsWorld->setGravity(btVector3(0, -5, 0));

    for (auto &rigidBodyPtr : m_RigidBodies) {
        m_DynamicsWorld->addRigidBody(rigidBodyPtr.get());
    }

    RegisterFixedUpdateFunction(std::bind(&Engine::PhysicsStep, this));
}

void Engine::DeinitPhysics() {
    if (m_DynamicsWorld) {
        while (m_DynamicsWorld->getNumConstraints() > 0) {
            m_DynamicsWorld->removeConstraint(m_DynamicsWorld->getConstraint(0));
        }

        btCollisionObjectArray &array = m_DynamicsWorld->getCollisionObjectArray();

        while (array.size() > 0) {
            btCollisionObject *obj = array[0];
            btRigidBody *rigidBody = btRigidBody::upcast(obj);

            if (rigidBody && rigidBody->getMotionState()) {
                delete rigidBody->getMotionState();
            }

            if (obj->getCollisionShape()) {
                delete obj->getCollisionShape();
            }

            m_DynamicsWorld->removeCollisionObject(obj);
            delete obj;
        }
    }

    m_DynamicsWorld.reset();
    m_Solver.reset();
    m_Broadphase.reset();
    m_Dispatcher.reset();
    m_CollisionConfig.reset();
}

void Engine::RegisterUIButtonListener(const std::function<void(std::string)>& listener) {
    m_UIButtonListeners.push_back(listener);
}

BaseRenderer *Engine::GetRenderer() {
    return m_Renderer;
}

void Engine::CheckButtonClicks(SDL_Event *event) {
    SDL_MouseButtonEvent *mouseButtonEvent = reinterpret_cast<SDL_MouseButtonEvent *>(event);
    glm::vec2 mousePos = glm::vec2(mouseButtonEvent->x, mouseButtonEvent->y)/glm::vec2(m_Settings->DisplayWidth, m_Settings->DisplayHeight);

    for (UI::Button *button : m_UIButtons) {
        glm::vec2 position = button->GetPosition();
        glm::vec2 scale = button->GetScale();

        if (position.x <= mousePos.x && mousePos.x <= position.x + scale.x &&
            position.y <= mousePos.y && mousePos.y <= position.y + scale.y) {
                for (auto listener : m_UIButtonListeners) {
                    listener(button->id);
                }
            }
    }
}

void Engine::Start() {
    using namespace std::chrono;

    high_resolution_clock::time_point lastLoopStartTime = high_resolution_clock::now(), loopStartTime = high_resolution_clock::now();

    bool shouldQuit = false;
    SDL_Event event;

    /* Each loop turn will add its delta time to this variable, and this will be used to execute certain tasks only every x seconds. */
    double accumulative = 0.0;

    while (!shouldQuit) {
        loopStartTime = high_resolution_clock::now();
        accumulative += duration_cast<duration<double>>(loopStartTime - lastLoopStartTime).count();

        while (SDL_PollEvent(&event)) {
            if (QuitEventCheck(event))
                shouldQuit = true;

            try {
                const auto &listeners = m_SDLEventToListenerMap.at(static_cast<SDL_EventType>(event.type));

                for (const auto &listener : listeners) {
                    listener(&event);
                }
            } catch (const std::out_of_range &e) {
                continue;
            }
        }

        for (const auto &func : m_UpdateFunctions) {
            func();
        }

        /* Fixed updates, every 60th of a second. */
        if (accumulative >= ENGINE_FIXED_UPDATE_DELTATIME) {
            accumulative -= ENGINE_FIXED_UPDATE_DELTATIME;

            for (const auto &fixedUpdateFunc : m_FixedUpdateFunctions) {
                fixedUpdateFunc();
            }
        }

        if (m_Renderer) {
            m_Renderer->StepRender();
        }

        lastLoopStartTime = loopStartTime;
    }
}

void Engine::LoadUIFile(const std::string &name) {
    if (m_Renderer == nullptr) {
        return;
    }

    const std::vector<UI::GenericElement *>& UIElements = UI::LoadUIFile(m_Renderer, name);
    for (UI::GenericElement *element : UIElements) {
        m_Renderer->AddUIGenericElement(element);

        if (element->type == UI::BUTTON) {
            RegisterUIButton(reinterpret_cast<UI::Button *>(element));
        }

        m_UIElements.push_back(element);

        std::vector<std::vector<UI::GenericElement *>> UIElementChildren {element->GetChildren()};

        /* Recursively check for elements to add and register (if they are buttons). */
        while (!UIElementChildren.empty()) {
            std::vector<UI::GenericElement *> &children = UIElementChildren[0];

            for (UI::GenericElement *child : children) {
                if (child->type == UI::BUTTON) {
                    RegisterUIButton(reinterpret_cast<UI::Button *>(child));
                }

                m_UIElements.push_back(child);

                UIElementChildren.push_back(child->GetChildren());
            }

            UIElementChildren.erase(UIElementChildren.begin());
        }
    }
}

void Engine::AddObject(Node *object) {
    fmt::println("Adding Object!");
    
    if (m_Renderer) {
        for (Model *model : object->GetModelAttachments()) {
            m_Renderer->LoadModel(model);
        }
    }

    if (object->GetCameraAttachment()) {
        m_Cameras.push_back(object->GetCameraAttachment());
    }

    if (object->GetRigidBody()) {
        auto &rigidBodyPtr = object->GetRigidBody();

        m_RigidBodies.push_back(rigidBodyPtr);

        if (m_DynamicsWorld) {
            m_DynamicsWorld->addRigidBody(rigidBodyPtr.get());
        }
    }

    for (Node *child : object->GetChildren()) {
        AddObject(child);
    }

    m_Objects.push_back(object);
}

std::vector<Camera *> &Engine::GetCameras() {
    return m_Cameras;
}

void Engine::RemoveCamera(Camera *cam) {
    const auto& camIt = std::find(m_Cameras.begin(), m_Cameras.end(), cam);

    if (camIt == m_Cameras.end()) {
        return;
    }

    if (m_MainCamera == cam) {
        m_MainCamera = nullptr;

        if (m_Renderer) {
            m_Renderer->SetPrimaryCamera(nullptr);
        }
    }

    m_Cameras.erase(camIt);
}

void Engine::RemoveObject(Node *object) {
    auto objectIt = std::find(m_Objects.begin(), m_Objects.end(), object);

    if (objectIt == m_Objects.end()) {
        return;
    }

    if (m_Renderer) {
        for (Model *model : object->GetModelAttachments()) {
            m_Renderer->UnloadModel(model);
        }
    }

    if (object->GetCameraAttachment()) {
        RemoveCamera(object->GetCameraAttachment());
    }

    for (Node *child : object->GetChildren()) {
        RemoveObject(child);
    }

    m_Objects.erase(objectIt);
}

/* Import an xml scene, overwriting the current one.
    * Throws std::runtime_error and rapidxml::parse_error

Input:
    - fileName, name of the XML scene file.

Output:
    - True if the scene was sucessfully imported.
*/
bool Engine::ImportScene(const std::string &path) {
    for (Object *object : m_Objects) {
        for (Model *model : object->GetModelAttachments()) {
            if (m_Renderer) {
                m_Renderer->UnloadModel(model);
            }

            delete model;
        }

        delete object;
    }
    m_Objects.clear();

    Node *rootObject = new Node();

    rootObject->ImportFromFile(path);

    AddObject(rootObject);

    m_ScenePath = path;

    return true;
}

/* TODO: Implement with assimp */
void Engine::ExportScene(const std::string &path) {
    Assimp::
}
//     using namespace rapidxml;

//     xml_document<char> sceneXML;

//     xml_node<char> *node = sceneXML.allocate_node(node_type::node_element, "Scene");
//     sceneXML.append_node(node);

//     for (Object *object : m_Objects) {
//         xml_node<char> *objectNode = sceneXML.allocate_node(node_type::node_element, "Object");
//         node->append_node(objectNode);

//         xml_node<char> *objectIDNode = sceneXML.allocate_node(node_type::node_element, "ObjectID", fmt::to_string(object->GetObjectID()).c_str());
//         objectNode->append_node(objectIDNode);
        
//         glm::vec3 position = object->GetPosition();
//         glm::vec3 rotation = object->GetRotation();
//         glm::vec3 scale = object->GetScale();

//         std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
//         xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
//         objectNode->append_node(positionNode);

//         std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
//         xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
//         objectNode->append_node(rotationNode);

//         std::string scaleStr = fmt::format("{} {} {}", scale.x, scale.y, scale.z);
//         xml_node<char> *scaleNode = sceneXML.allocate_node(node_type::node_element, "Scale", sceneXML.allocate_string(scaleStr.c_str()));
//         objectNode->append_node(scaleNode);

//         for (Model *model : object->GetModelAttachments()) {
//             xml_node<char> *modelNode = sceneXML.allocate_node(node_type::node_element, "Model");
//             objectNode->append_node(modelNode);

//             glm::vec3 position = model->GetPosition();
//             glm::vec3 rotation = model->GetRotation();
//             glm::vec3 scale = model->GetScale();

//             std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
//             xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
//             modelNode->append_node(positionNode);

//             std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
//             xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
//             modelNode->append_node(rotationNode);

//             std::string scaleStr = fmt::format("{} {} {}", scale.x, scale.y, scale.z);
//             xml_node<char> *scaleNode = sceneXML.allocate_node(node_type::node_element, "Scale", sceneXML.allocate_string(scaleStr.c_str()));
//             modelNode->append_node(scaleNode);

//             for (Mesh &mesh : model->meshes) {
//                 xml_node<char> *meshNode = sceneXML.allocate_node(node_type::node_element, "Mesh");
//                 modelNode->append_node(meshNode);

//                 std::string diffuseStr = fmt::format("{} {} {}", mesh.diffuse.x, mesh.diffuse.y, mesh.diffuse.z);
//                 xml_node<char> *diffuseNode = sceneXML.allocate_node(node_type::node_element, "Diffuse", sceneXML.allocate_string(diffuseStr.c_str()));
//                 meshNode->append_node(diffuseNode);

//                 std::string indicesStr = fmt::to_string(fmt::join(mesh.indices, ","));
//                 xml_node<char> *indicesNode = sceneXML.allocate_node(node_type::node_element, "Indices", sceneXML.allocate_string(indicesStr.c_str()));
//                 meshNode->append_node(indicesNode);

//                 xml_node<char> *diffuseMapPathNode = sceneXML.allocate_node(node_type::node_element, "DiffuseMap", mesh.diffuseMapPath.c_str());
//                 meshNode->append_node(diffuseMapPathNode);

//                 for (Vertex vert : mesh.vertices) {
//                     xml_node<char> *vertexNode = sceneXML.allocate_node(node_type::node_element, "Vertex");
//                     meshNode->append_node(vertexNode);

//                     std::string vertPositionStr = fmt::format("{} {} {}", vert.Position.x, vert.Position.y, vert.Position.z);
//                     xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(vertPositionStr.c_str()));
//                     vertexNode->append_node(positionNode);
                    
//                     std::string vertNormalStr = fmt::format("{} {} {}", vert.Normal.x, vert.Normal.y, vert.Normal.z);
//                     xml_node<char> *normalNode = sceneXML.allocate_node(node_type::node_element, "Normal", sceneXML.allocate_string(vertNormalStr.c_str()));
//                     vertexNode->append_node(normalNode);
                    
//                     std::string vertTexCoordStr = fmt::format("{} {}", vert.TexCoord.x, vert.TexCoord.y);
//                     xml_node<char> *texCoordNode = sceneXML.allocate_node(node_type::node_element, "TexCoord", sceneXML.allocate_string(vertTexCoordStr.c_str()));
//                     vertexNode->append_node(texCoordNode);
//                 }
//             }
//         }
//     }

//     std::ofstream targetFile(path);
//     targetFile << sceneXML;

//     sceneXML.clear();
// }

UI::GenericElement *Engine::GetElementByID(const std::string &id) {
    for (UI::GenericElement *&element : m_UIElements) {
        if (element->id == id) {
            return element;
        }
    }

    return nullptr;
}

void Engine::RegisterUpdateFunction(const std::function<void()> &func) {
    m_UpdateFunctions.push_back(func);
}

void Engine::RegisterFixedUpdateFunction(const std::function<void()> &func) {
    m_FixedUpdateFunctions.push_back(func);
}

void Engine::RegisterUIButton(UI::Button *button) {
    m_UIButtons.push_back(button);
}

void Engine::UnregisterUIButton(UI::Button *button) {
    auto buttonIter = std::find(m_UIButtons.begin(), m_UIButtons.end(), button);

    if (buttonIter != m_UIButtons.end()) {
        m_UIButtons.erase(buttonIter);
    }

    return;
}

void Engine::RegisterSDLEventListener(const std::function<void(SDL_Event *)> &func, SDL_EventType types) {
    if (m_SDLEventToListenerMap.find(types) == m_SDLEventToListenerMap.end()) {
        m_SDLEventToListenerMap.insert(std::make_pair(types, std::vector<std::function<void(SDL_Event *)>>()));
    }
    
    m_SDLEventToListenerMap[types].push_back(func);
}

Node *Engine::GetObjectByID(int ObjectID) {
    auto it = std::find_if(m_Objects.begin(), m_Objects.end(), [ObjectID] (Object *&obj) { return ObjectID == obj->GetObjectID(); });

    if (it == m_Objects.end()) {
        return nullptr;
    }

    return *it;
}

void Engine::PhysicsStep() {
    if (!m_DynamicsWorld) {
        return;
    }

    m_DynamicsWorld->stepSimulation(ENGINE_FIXED_UPDATE_DELTATIME, 4, ENGINE_FIXED_UPDATE_DELTATIME / 4.0f);

    /* TODO: in the far future, we might be able to do softbodies! */
    for (int i = m_DynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
        btCollisionObject *collisionObject = m_DynamicsWorld->getCollisionObjectArray()[i];
        btRigidBody *body = btRigidBody::upcast(collisionObject);

        btTransform transform;

        if (body && body->getMotionState()) {
            body->getMotionState()->getWorldTransform(transform);
        } else {
            transform = collisionObject->getWorldTransform();
        }

        btVector3 origin = transform.getOrigin();
        btQuaternion rotation = transform.getRotation();

        Node *obj = reinterpret_cast<Node *>(body->getUserPointer());
        UTILASSERT(obj);

        obj->SetPosition(glm::vec3(origin.getX(), origin.getY(), origin.getZ()));
        obj->SetRotation(glm::quat(rotation.getW(), rotation.getX(), rotation.getY(), rotation.getZ()));
    }
}
