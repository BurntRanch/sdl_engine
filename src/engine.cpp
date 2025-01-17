#include "engine.hpp"

#include "BulletCollision/BroadphaseCollision/btDbvtBroadphase.h"
#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionConfiguration.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#include "BulletDynamics/Dynamics/btDynamicsWorld.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btVector3.h"
#include "camera.hpp"
#include "common.hpp"
#include "fmt/base.h"
#include "error.hpp"
#include "fmt/format.h"
#include "isteamnetworkingsockets.h"
#include "networking/connection.hpp"
#include "object.hpp"
#include "renderer/vulkanRenderer.hpp"
#include "steamclientpublic.h"
#include "steamnetworkingsockets.h"
#include "model.hpp"
#include "steamnetworkingtypes.h"
#include "steamtypes.h"
#include "ui/arrows.hpp"
#include "ui/button.hpp"
#include "ui/label.hpp"
#include "ui/panel.hpp"
#include "ui/waypoint.hpp"
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
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <glm/fwd.hpp>
#include <iterator>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

Engine::Engine() {
    SDL_Init(SDL_INIT_EVENTS);
}

Engine::~Engine() {
    for (NetworkingThreadState &state : m_NetworkingThreadStates) {
        if (state.status != NETWORKING_THREAD_INACTIVE) {
            state.shouldQuit = true;
            state.thread.join();
        }
    }

    GameNetworkingSockets_Kill();

    DeinitPhysics();
}

void Engine::InitRenderer(Settings &settings, Camera *primaryCamera) {
    m_Settings = &settings;

    m_Renderer = new VulkanRenderer(settings, primaryCamera);

    m_Renderer->Init();

    RegisterSDLEventListener(std::bind(&Engine::CheckButtonClicks, this, std::placeholders::_1), SDL_EVENT_MOUSE_BUTTON_UP);
    RegisterUpdateFunction(std::bind(&Engine::ProcessNetworkEvents, this, &m_NetworkingEvents));
}

void Engine::InitNetworking() {
    SteamDatagramErrMsg errMsg;

    if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
        throw std::runtime_error(fmt::format("Failed to initialize networking! {}", errMsg));
    }

    m_NetworkingSockets = SteamNetworkingSockets();
}

void Engine::InitPhysics() {
    m_CollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    m_Dispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfig.get());
    m_Broadphase = std::make_unique<btDbvtBroadphase>();
    m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();

    m_DynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(m_Dispatcher.get(), m_Broadphase.get(), m_Solver.get(), m_CollisionConfig.get());

    m_DynamicsWorld->setGravity(btVector3(0, -0.25, 0));

    for (auto &rigidBodyPtr : m_RigidBodies) {
        m_DynamicsWorld->addRigidBody(rigidBodyPtr.get());
    }

    RegisterTickUpdateHandler(std::bind(&Engine::PhysicsStep, this, std::placeholders::_1), NETWORKING_THREAD_ACTIVE_SERVER);
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

void Engine::RegisterUIButtonListener(const std::function<void(std::string)> listener) {
    m_UIButtonListeners.push_back(listener);
}

void Engine::RegisterTickUpdateHandler(const std::function<void(int)> handler, NetworkingThreadStatus status) {
    NetworkingThreadState *state = nullptr;

    switch (status) {
        case NETWORKING_THREAD_ACTIVE_CLIENT:
            state = &m_NetworkingThreadStates[0];
            break;
        case NETWORKING_THREAD_ACTIVE_SERVER:
            state = &m_NetworkingThreadStates[1];
            break;
        case NETWORKING_THREAD_INACTIVE:
        default:
            break;
    }

    if (state != nullptr) {
        state->tickUpdateHandlers.push_back(handler);
    } else if (status == NETWORKING_THREAD_ACTIVE_BOTH) {
        for (NetworkingThreadState &state : m_NetworkingThreadStates) {
            state.tickUpdateHandlers.push_back(handler);
        }
    }
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

    high_resolution_clock::time_point lastLoopStartTime, loopStartTime = high_resolution_clock::now();
    bool shouldQuit = false;
    SDL_Event event;

    /* Each loop turn will add its delta time to this variable, and this will be used to execute certain tasks only every x seconds. */
    float accumulative = 0.f;

    while (!shouldQuit) {
        loopStartTime = high_resolution_clock::now();
        accumulative += duration_cast<seconds>(lastLoopStartTime - loopStartTime).count();

        while (SDL_PollEvent(&event)) {
            if (QuitEventCheck(event))
                shouldQuit = true;

            try {
                auto &listeners = m_SDLEventToListenerMap.at((SDL_EventType)(event.type));

                for (auto &listener : listeners) {
                    listener(&event);
                }
            } catch (const std::out_of_range &e) {
                continue;
            }
        }

        for (auto &func : m_UpdateFunctions) {
            func();
        }

        /* Fixed updates, every 60th of a second. */
        if (accumulative > ENGINE_FIXED_UPDATE_DELTATIME) {
            accumulative -= ENGINE_FIXED_UPDATE_DELTATIME;

            for (auto &fixedUpdateFunc : m_FixedUpdateFunctions) {
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

    std::vector<UI::GenericElement *> UIElements = UI::LoadUIFile(m_Renderer, name);
    for (UI::GenericElement *element : UIElements) {
        m_Renderer->AddUIGenericElement(element);

        if (element->type == UI::BUTTON) {
            RegisterUIButton(reinterpret_cast<UI::Button *>(element));
        }

        m_UIElements.push_back(element);

        std::vector<std::vector<UI::GenericElement *>> UIElementChildren = {element->GetChildren()};

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

void Engine::AddObject(Object *object) {
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

    for (Object *child : object->GetChildren()) {
        AddObject(child);
    }

    m_Objects.push_back(object);
}

std::vector<Camera *> &Engine::GetCameras() {
    return m_Cameras;
}

void Engine::RemoveCamera(Camera *cam) {
    auto camIt = std::find(m_Cameras.begin(), m_Cameras.end(), cam);

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

void Engine::RemoveObject(Object *object) {
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

    for (Object *child : object->GetChildren()) {
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

    Object *rootObject = new Object();

    rootObject->ImportFromFile(path);

    AddObject(rootObject);

    m_ScenePath = path;

    return true;
}

/* TODO: Implement with assimp */
// void Engine::ExportScene(const std::string &path) {
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

void Engine::AttachCameraToConnection(Camera *cam, SteamConnection &conn) {
    m_ConnToCameraAttachment[&conn] = cam;
}

void Engine::ConnectToGameServer(SteamNetworkingIPAddr ipAddr) {
    SteamNetworkingConfigValue_t opt{};    

    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)onConnectionStatusChangedCallback);

    HSteamNetConnection netConnection = m_NetworkingSockets->ConnectByIPAddress(ipAddr, 1, &opt);
    
    if (netConnection == k_HSteamNetConnection_Invalid) {
        throw std::runtime_error("Failed to connect to server!");
    }

    InitNetworkingThread(NETWORKING_THREAD_ACTIVE_CLIENT);
}

void Engine::HostGameServer(SteamNetworkingIPAddr ipAddr) {
    SteamNetworkingConfigValue_t opt{};
    
    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)(onConnectionStatusChangedCallback));

    m_NetListenSocket = m_NetworkingSockets->CreateListenSocketIP(ipAddr, 1, &opt);

    if (m_NetListenSocket == k_HSteamListenSocket_Invalid) {
        throw std::runtime_error("Failed to create listen socket!");
    }

    m_NetPollGroup = m_NetworkingSockets->CreatePollGroup();

    if (m_NetPollGroup == k_HSteamNetPollGroup_Invalid) {
        throw std::runtime_error("Failed to create poll group!");
    }

    fmt::println("Created a listen socket!");

    InitNetworkingThread(NETWORKING_THREAD_ACTIVE_SERVER);
}

void Engine::ConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *callbackInfo) {
    fmt::println("Connection status changed!! ({})", (int)callbackInfo->m_info.m_eState);

    switch (callbackInfo->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:
            break;
        
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            if (callbackInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
			{
                if (m_NetworkingThreadStates[1].status & NETWORKING_THREAD_ACTIVE_SERVER) {
                    NetworkingThreadState &state = m_NetworkingThreadStates[1];

                    auto conn = std::find(state.connections.begin(), state.connections.end(), callbackInfo->m_hConn);
                    UTILASSERT(conn != state.connections.end());

                    FireNetworkEvent(EVENT_CLIENT_DISCONNECTED, *conn);

                    state.connections.erase(conn);
                /* if its a client */
                } else {
                    UTILASSERT(m_NetworkingThreadStates[0].status & NETWORKING_THREAD_ACTIVE_CLIENT);

                    NetworkingThreadState &state = m_NetworkingThreadStates[0];
                    UTILASSERT(state.connections.size() == 1 && state.connections[0] == callbackInfo->m_hConn);

                    FireNetworkEvent(EVENT_DISCONNECTED_FROM_SERVER, state.connections[0]);

                    state.connections.erase(state.connections.begin());
                }

                m_NetworkingSockets->CloseConnection(callbackInfo->m_hConn, 0, nullptr, false);
            }

            break;
        case k_ESteamNetworkingConnectionState_Connecting:
            if (m_NetworkingThreadStates[1].status & NETWORKING_THREAD_ACTIVE_SERVER) {
                fmt::println("We're getting a connection!");

                NetworkingThreadState &state = m_NetworkingThreadStates[1];

                /* This callback only happens when a new client is connecting. */
                UTILASSERT(std::find(state.connections.begin(), state.connections.end(), callbackInfo->m_hConn) == state.connections.end());

                if (m_NetworkingSockets->AcceptConnection(callbackInfo->m_hConn) != k_EResultOK) {
                    m_NetworkingSockets->CloseConnection(callbackInfo->m_hConn, 0, nullptr, false);

                    break;
                }
                
                if (!m_NetworkingSockets->SetConnectionPollGroup(callbackInfo->m_hConn, m_NetPollGroup)) {
                    m_NetworkingSockets->CloseConnection(callbackInfo->m_hConn, 0, nullptr, false);

                    break;
                }

                SteamConnection connection(m_NetworkingSockets, callbackInfo->m_hConn);

                FireNetworkEvent(EVENT_CLIENT_CONNECTED, connection);

                SendFullUpdateToConnection(connection, state.tickNumber);

                state.connections.push_back(connection);
            }

            break;
        
        case k_ESteamNetworkingConnectionState_Connected:
            if (m_NetworkingThreadStates[0].status & NETWORKING_THREAD_ACTIVE_CLIENT) {
                UTILASSERT(m_NetworkingThreadStates[0].connections.size() < 1);

                SteamConnection conn(m_NetworkingSockets, callbackInfo->m_hConn);

                m_NetworkingThreadStates[0].connections.push_back(conn);

                FireNetworkEvent(EVENT_CONNECTED_TO_SERVER, conn);
            }

            break;

        default:
            break;
    }
}

void Engine::RegisterNetworkEventListener(const std::function<void(SteamConnection &)> listener, NetworkingEventType listenerTarget) {
    if (m_EventTypeToListenerMap.find(listenerTarget) == m_EventTypeToListenerMap.end()) {
        m_EventTypeToListenerMap[listenerTarget] = std::vector<std::function<void(SteamConnection &)>>();
    }

    m_EventTypeToListenerMap[listenerTarget].push_back(listener);
}


void Engine::RegisterNetworkDataListener(const std::function<void(SteamConnection &, std::vector<std::byte> &)> listener) {
    m_DataListeners.push_back(listener);
}

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

void Engine::InitNetworkingThread(NetworkingThreadStatus status) {
    fmt::println("Initializing network thread to {}!", (int)status);

    if (status == NETWORKING_THREAD_ACTIVE_CLIENT) {
        NetworkingThreadState &state = m_NetworkingThreadStates[0];

        if (state.status != NETWORKING_THREAD_INACTIVE) {
            return;
        }

        state.thread = std::thread(&Engine::NetworkingThreadClient_Main, this, std::ref(state));
    } else if (status == NETWORKING_THREAD_ACTIVE_SERVER) {
        NetworkingThreadState &state = m_NetworkingThreadStates[1];

        if (state.status != NETWORKING_THREAD_INACTIVE) {
            return;
        }

        state.thread = std::thread(&Engine::NetworkingThreadServer_Main, this, std::ref(state));
    }
}

void Engine::NetworkingThreadClient_Main(NetworkingThreadState &state) {
    fmt::println("Started client networking thread!");
    state.status |= NETWORKING_THREAD_ACTIVE_CLIENT;

    using namespace std::chrono;

    high_resolution_clock::time_point lastTickTime = high_resolution_clock::now();
    double accumulativeTickTime = 0;

    while (!state.shouldQuit) {
        high_resolution_clock::time_point now = high_resolution_clock::now();

        double tickDeltaTime = (duration_cast<duration<double>>(now - lastTickTime).count());

        accumulativeTickTime += tickDeltaTime;

        /* hardcoded 64 tick, to be replaced. */
        if (accumulativeTickTime < (1.0f/64.0f)) {
            std::this_thread::sleep_for(milliseconds(8));  /* keep waking up at half the time just incase */
            continue;
        }

        accumulativeTickTime -= (1.0f / 64.0f);
        
        if (state.tickNumber != -1) {
            if (state.tickNumber > state.predictionTickNumber) {
                state.predictionTickNumber = state.tickNumber;
            }

            state.predictionTickNumber++;

            for (auto handler : state.tickUpdateHandlers) {
                handler(state.predictionTickNumber);
            }
        }

        m_CallbackInstance = this;
        m_NetworkingSockets->RunCallbacks();

        if (state.connections.size() >= 1) {
            /* Receiving */
            std::vector<std::vector<std::byte>> incomingMessages = state.connections[0].ReceiveMessages(1);

            for (auto &incomingMessage : incomingMessages) {
                if (incomingMessage.size() < sizeof(size_t)) {
                    fmt::println("Invalid packet!");
                    continue;
                }

                Networking_StatePacket packet = DeserializePacket(incomingMessage);
#ifdef LOG_FRAME
                fmt::println("New state packet just dropped! {} objects sent by server", packet.objects.size());
#endif
                /* add a dummy type */
                Networking_Event event{NETWORKING_NULL, {}, {}, {}};
                std::lock_guard<std::mutex> networkingEventsLockGuard(m_NetworkingEventsLock);

                state.tickNumber = packet.tickNumber;

                if (state.lastSyncedTickNumber == -1) {
                    event.type = NETWORKING_INITIAL_UPDATE;
                    event.packet = packet;

                    m_NetworkingEvents.push_back(event);
                } else {
                    for (Networking_Object &networkingObject : packet.objects) {
                        auto it = std::find_if(m_Objects.begin(), m_Objects.end(), [networkingObject] (Object *obj) { return obj->GetObjectID() == networkingObject.ObjectID; });

                        event.object = networkingObject;

                        /* If the scene changed, we should wait until the scene is properly loaded */
                        if (it == m_Objects.end()) {
                            event.type = NETWORKING_NEW_OBJECT;

                            m_NetworkingEvents.push_back(event);

                            continue;
                        }

                        // Object *obj = m_Objects.at(std::distance(m_Objects.begin(), it));

                        // UTILASSERT(obj);

                        event.type = NETWORKING_UPDATE_OBJECT;

                        m_NetworkingEvents.push_back(event);
                    }
                }

                state.lastSyncedTickNumber = packet.tickNumber;
            }
        }


        /* Sending */
        // ...
    }

    fmt::println("Stopping client networking thread!");

    DisconnectFromServer();
    state.status &= ~NETWORKING_THREAD_ACTIVE_CLIENT;
    state.lastSyncedTickNumber = -1;
    state.tickNumber = -1;
    state.shouldQuit = false;
}



void Engine::NetworkingThreadServer_Main(NetworkingThreadState &state) {
    if (!m_NetListenSocket) {
        throw std::runtime_error("Networking Thread initialized with no networking connection!");
    }

    fmt::println("Started server networking thread!");
    state.status |= NETWORKING_THREAD_ACTIVE_SERVER;

    using namespace std::chrono;

    high_resolution_clock::time_point lastTickTime = high_resolution_clock::now();
    double accumulativeTickTime = 0;

    while (!state.shouldQuit) {
        high_resolution_clock::time_point now = high_resolution_clock::now();

        double tickDeltaTime = (duration_cast<duration<double>>(now - lastTickTime).count());

        accumulativeTickTime += tickDeltaTime;

        /* hardcoded 64 tick, to be replaced. */
        if (accumulativeTickTime < (1.0f/64.0f)) {
            std::this_thread::sleep_for(milliseconds(8));  /* keep waking up at half the time just incase */
            continue;
        }

        accumulativeTickTime -= (1.0f / 64.0f);

        state.tickNumber++;
        
        for (auto handler : state.tickUpdateHandlers) {
            handler(state.tickNumber);
        }

        m_CallbackInstance = this;
        m_NetworkingSockets->RunCallbacks();

        ISteamNetworkingMessage *incomingMessages;

        /* TODO: This stupid thing doesn't receive client requests. */
        int msgCount = m_NetworkingSockets->ReceiveMessagesOnPollGroup(m_NetPollGroup, &incomingMessages, 1);
        
        if (msgCount < 0) {
            throw std::runtime_error("Error receiving messages from a client!");
        }
        if (msgCount > 0) {
            for (int i = 0; i < msgCount; i++) {
                ISteamNetworkingMessage *incomingMessage = &incomingMessages[i];

                if (incomingMessage->GetSize() < sizeof(int)) {
                    fmt::println("Invalid packet!");
                } else {
                    fmt::println("DATA!");

                    const void *data = incomingMessage->GetData();
                    
                    std::vector<std::byte> message{reinterpret_cast<const std::byte *>(data), reinterpret_cast<const std::byte *>(data) + incomingMessage->GetSize()};

                    Networking_ClientRequest packet;

                    DeserializeClientRequest(message, packet);

                    auto connection = std::find(state.connections.begin(), state.connections.end(), incomingMessage->GetConnection());
                    UTILASSERT(connection != state.connections.end());

                    switch (packet.requestType) {
                        case CLIENT_REQUEST_DISCONNECT:
                            DisconnectClientFromServer(*connection);
                            break;
                        case CLIENT_REQUEST_APPLICATION:
                            FireNetworkEvent(EVENT_RECEIVED_CLIENT_REQUEST, *connection, packet.data);
                            break;
                    }
                }
                
                incomingMessage->Release();
            }
        }

        for (SteamConnection &connection : state.connections) {
            SendUpdateToConnection(connection, state.tickNumber);
        }

        lastTickTime = now;
    }

    fmt::println("Stopping server networking thread!");

    StopHostingGameServer();
    state.status &= ~NETWORKING_THREAD_ACTIVE_SERVER;
    state.lastSyncedTickNumber = -1;
    state.tickNumber = -1;
    state.shouldQuit = false;
}

void Engine::DisconnectFromServer() {
    NetworkingThreadState &state = m_NetworkingThreadStates[0];

    if (state.connections.empty()) {
        return;
    }

    UTILASSERT(state.connections.size() == 1);

    FireNetworkEvent(EVENT_DISCONNECTED_FROM_SERVER, state.connections[0]);

    /* TODO: this should work but idk why the server doesn't receive anything. */
    std::vector<std::byte> serializedDisconnectRequest;
    Networking_ClientRequest clientRequest{CLIENT_REQUEST_DISCONNECT};
    SerializeClientRequest(clientRequest, serializedDisconnectRequest);
    state.connections[0].SendMessage(serializedDisconnectRequest, k_nSteamNetworkingSend_Reliable);

    state.connections.erase(state.connections.begin());
}

void Engine::DisconnectClientFromServer(SteamConnection &connection) {
    NetworkingThreadState &state = m_NetworkingThreadStates[1];

    auto it = std::find(state.connections.begin(), state.connections.end(), connection);
    UTILASSERT(it != state.connections.end());

    FireNetworkEvent(EVENT_CLIENT_DISCONNECTED, connection);

    state.connections.erase(it);
}

void Engine::StopHostingGameServer() {
    NetworkingThreadState &state = m_NetworkingThreadStates[1];

    if (state.status & NETWORKING_THREAD_ACTIVE_SERVER) {
        while (state.connections.size() != 0) {
            DisconnectClientFromServer(state.connections[0]);
        }

        if (m_NetListenSocket != k_HSteamListenSocket_Invalid) {
            m_NetworkingSockets->CloseListenSocket(m_NetListenSocket);
        }
    }
}

void Engine::ProcessNetworkEvents(std::vector<Networking_Event> *networkingEvents) {
    std::unique_lock<std::mutex> networkingEventsLockGuard(m_NetworkingEventsLock);

    /* only for initial updates */
    std::vector<Networking_Event> newEvents;

    while (!networkingEvents->empty()) {
        Networking_Event &event = (*networkingEvents)[0];

        Object *object;
        Camera *camera;

        Networking_Object objectPacket;
        Networking_Camera cameraPacket;

        switch (event.type) {
            case NETWORKING_INITIAL_UPDATE:
                /* event.packet MUST be set, This will automatically error out for us in the rare case of it not actually being set. */
                for (Networking_Camera &camera : event.packet.value().cameras) {
                    Networking_Event event;

                    event.type = NETWORKING_NEW_CAMERA;
                    event.camera = camera;

                    newEvents.push_back(event);
                }

                for (Networking_Object &object : event.packet.value().objects) {
                    Networking_Event event;

                    event.type = NETWORKING_NEW_OBJECT;
                    event.object = object;

                    newEvents.push_back(event);
                }

                networkingEventsLockGuard.unlock();
                ProcessNetworkEvents(&newEvents);
                networkingEventsLockGuard.lock();

                break;
            case NETWORKING_NEW_CAMERA:
                cameraPacket = event.camera.value();

                camera = new Camera(cameraPacket.aspectRatio, cameraPacket.up, cameraPacket.yaw, cameraPacket.pitch);

                camera->SetCameraID(cameraPacket.cameraID);

                if (cameraPacket.isOrthographic) {
                    camera->type = CAMERA_ORTHOGRAPHIC;

                    camera->OrthographicWidth = cameraPacket.orthographicWidth;
                }

                camera->AspectRatio = cameraPacket.aspectRatio;
                
                camera->Pitch = cameraPacket.pitch;
                camera->Yaw = cameraPacket.yaw;

                camera->Up = cameraPacket.up;

                camera->FOV = cameraPacket.fov;
                
                if (cameraPacket.isMainCamera && !m_MainCamera) {
                    m_MainCamera = camera;
                    m_Renderer->SetPrimaryCamera(camera);
                }

                m_Cameras.push_back(camera);

                break;
            case NETWORKING_NEW_OBJECT:
                object = new Object();

                objectPacket = event.object.value();

                if (objectPacket.isGeneratedFromFile && objectPacket.objectSourceFile != "") {
                    std::filesystem::path path = objectPacket.objectSourceFile;

                    std::string absoluteSourcePath = std::filesystem::absolute(path).string();
                    std::string absoluteResourcesPath = std::filesystem::absolute("resources").string();

                    UTILASSERT(absoluteSourcePath.substr(0, absoluteResourcesPath.length()).compare(absoluteResourcesPath) == 0);

                    object->ImportFromFile(absoluteSourcePath);

                    std::vector<std::pair<Networking_Object *, int>> relatedObjects = FilterRelatedNetworkingObjects(m_ObjectsFromImportedObject, &objectPacket);

                    int offset = 0;
                    for (auto &generatedObjectPair : relatedObjects) {
                        Networking_Object *generatedObject = generatedObjectPair.first;

                        Object *childEquivalent = DeepSearchObjectTree(object, [generatedObject] (Object *child) { return child->GetSourceID() == generatedObject->objectSourceID; });
                        UTILASSERT(childEquivalent);

                        childEquivalent->SetObjectID(generatedObject->ObjectID);

                        childEquivalent->SetPosition(generatedObject->position);
                        childEquivalent->SetRotation(generatedObject->rotation);
                        childEquivalent->SetScale(generatedObject->scale);

                        /* assuming its not all generated */
                        for (int &childID : generatedObject->children) {
                            auto previousObjectPtr = std::find_if(m_PreviousObjects.begin(), m_PreviousObjects.end(), [childID] (Object *&packet) { return packet->GetObjectID() == childID; });

                            if (previousObjectPtr == m_PreviousObjects.end()) {
                                continue;
                            }

                            Object *previousObject = *previousObjectPtr;

                            previousObject->SetParent(childEquivalent);

                            m_PreviousObjects.erase(previousObjectPtr);
                        }

                        if (childEquivalent->GetCameraAttachment() != nullptr) {
                            for (Camera *cam : m_Cameras) {
                                if (cam->GetCameraID() == generatedObject->cameraAttachment) {
                                    Camera *oldCamera = childEquivalent->GetCameraAttachment();

                                    childEquivalent->SetCameraAttachment(cam);

                                    RemoveCamera(oldCamera);

                                    delete oldCamera;
                                }
                            }
                        }
                        
                        m_ObjectsFromImportedObject.erase(m_ObjectsFromImportedObject.begin() + (generatedObjectPair.second - offset++));
                    }
                } else if (objectPacket.isGeneratedFromFile) {
                    m_ObjectsFromImportedObject.push_back(objectPacket);
                    
                    networkingEvents->erase(networkingEvents->begin());
                    continue;
                }

                object->SetObjectID(objectPacket.ObjectID);

                object->SetPosition(objectPacket.position);
                object->SetRotation(objectPacket.rotation);
                object->SetScale(objectPacket.scale);

                /* assuming its not all generated */
                for (int &childID : objectPacket.children) {
                    auto previousObjectPtr = std::find_if(m_PreviousObjects.begin(), m_PreviousObjects.end(), [childID] (Object *&packet) { return packet->GetObjectID() == childID; });

                    if (previousObjectPtr == m_PreviousObjects.end()) {
                        continue;
                    }

                    Object *previousObject = *previousObjectPtr;

                    previousObject->SetParent(object);

                    m_PreviousObjects.erase(previousObjectPtr);
                }

                if (object->GetCameraAttachment() != nullptr) {
                    for (Camera *cam : m_Cameras) {
                        if (cam->GetCameraID() == objectPacket.cameraAttachment) {
                            object->SetCameraAttachment(cam);
                        }
                    }
                }

                m_PreviousObjects.push_back(object);

                AddObject(object);

                break;
            case NETWORKING_UPDATE_OBJECT:
                objectPacket = event.object.value();

                object = GetObjectByID(objectPacket.ObjectID);
                UTILASSERT(object);

                object->SetPosition(objectPacket.position);
                object->SetRotation(objectPacket.rotation);
                object->SetScale(objectPacket.scale);

                /* TODO: inheritance, and tons of other stuff to synchronize. */
                /* idea: perhaps move inheritance to a stateful class that's able to keep track of all previous Networking_Objects */

                break;
            default:
                break;
        }

        networkingEvents->erase(networkingEvents->begin());
    }
}

bool Engine::IsConnectedToGameServer() {
    return m_NetworkingThreadStates[0].connections.size() == 1;
}

void Engine::RegisterSDLEventListener(const std::function<void(SDL_Event *)> &func, SDL_EventType types) {
    if (m_SDLEventToListenerMap.find(types) == m_SDLEventToListenerMap.end()) {
        m_SDLEventToListenerMap.insert(std::make_pair(types, std::vector<std::function<void(SDL_Event *)>>()));
    }
    
    m_SDLEventToListenerMap[types].push_back(func);
}

void Engine::SendRequestToServer(std::vector<std::byte> &data) {
    NetworkingThreadState &state = m_NetworkingThreadStates[0];

    if (state.status == NETWORKING_THREAD_INACTIVE) {
        return;
    }

    UTILASSERT(state.connections.size() == 1);

    Networking_ClientRequest request{};
    request.requestType = CLIENT_REQUEST_APPLICATION;
    request.data = data;

    std::vector<std::byte> serializedRequest;
    SerializeClientRequest(request, serializedRequest);

    state.connections[0].SendMessage(serializedRequest, k_nSteamNetworkingSend_Reliable);
}

Object *Engine::GetObjectByID(int ObjectID) {
    auto it = std::find_if(m_Objects.begin(), m_Objects.end(), [ObjectID] (Object *&obj) { return ObjectID == obj->GetObjectID(); });

    if (it == m_Objects.end()) {
        return nullptr;
    }

    return *it;
}

Networking_StatePacket Engine::DeserializePacket(std::vector<std::byte> &serializedPacket) {
    Networking_StatePacket statePacket{};

    Deserialize(serializedPacket, statePacket.tickNumber);

    size_t camerasCount;
    Deserialize(serializedPacket, camerasCount);

    for (size_t i = 0; i < camerasCount; i++) {
        Networking_Camera cameraPacket;

        DeserializeNetworkingCamera(serializedPacket, cameraPacket);

        statePacket.cameras.push_back(cameraPacket);
    }

    size_t objectsCount;
    Deserialize(serializedPacket, objectsCount);

    for (size_t i = 0; i < objectsCount; i++) {
        Networking_Object objectPacket;

        DeserializeNetworkingObject(serializedPacket, objectPacket);

        statePacket.objects.push_back(objectPacket);
    }

    return statePacket;
}

void Engine::DeserializeNetworkingObject(std::vector<std::byte> &serializedObjectPacket, Networking_Object &dest) {
    Deserialize(serializedObjectPacket, dest.ObjectID);

    Deserialize(serializedObjectPacket, dest.position.x);
    Deserialize(serializedObjectPacket, dest.position.y);
    Deserialize(serializedObjectPacket, dest.position.z);

    Deserialize(serializedObjectPacket, dest.rotation.x);
    Deserialize(serializedObjectPacket, dest.rotation.y);
    Deserialize(serializedObjectPacket, dest.rotation.z);
    Deserialize(serializedObjectPacket, dest.rotation.w);

    Deserialize(serializedObjectPacket, dest.scale.x);
    Deserialize(serializedObjectPacket, dest.scale.y);
    Deserialize(serializedObjectPacket, dest.scale.z);

    Deserialize(serializedObjectPacket, dest.isGeneratedFromFile);

    if (dest.isGeneratedFromFile) {
        Deserialize(serializedObjectPacket, dest.objectSourceFile);
        Deserialize(serializedObjectPacket, dest.objectSourceID);
    }

    size_t childrenListSize;
    Deserialize(serializedObjectPacket, childrenListSize);

    for (size_t i = 0; i < childrenListSize; i++) {
        int childObjectID;
        Deserialize(serializedObjectPacket, childObjectID);

        dest.children.push_back(childObjectID);
    }

    Deserialize(serializedObjectPacket, dest.cameraAttachment);
}

void Engine::DeserializeNetworkingCamera(std::vector<std::byte> &serializedCameraPacket, Networking_Camera &dest) {
    Deserialize(serializedCameraPacket, dest.cameraID);

    Deserialize(serializedCameraPacket, dest.isOrthographic);

    Deserialize(serializedCameraPacket, dest.aspectRatio);
    Deserialize(serializedCameraPacket, dest.orthographicWidth);

    Deserialize(serializedCameraPacket, dest.pitch);
    Deserialize(serializedCameraPacket, dest.yaw);

    Deserialize(serializedCameraPacket, dest.up.x);
    Deserialize(serializedCameraPacket, dest.up.y);

    Deserialize(serializedCameraPacket, dest.fov);
    Deserialize(serializedCameraPacket, dest.isMainCamera);
}

void Engine::PhysicsStep(int _) {
    if (!m_DynamicsWorld) {
        return;
    }

    m_DynamicsWorld->stepSimulation(1.0f / 64.0f, 4, (1.0f / 4.0f) / 64.0f);

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

        fmt::println("origin: {} {} {}", origin.getX(), origin.getY(), origin.getZ());

        Object *obj = reinterpret_cast<Object *>(collisionObject->getUserPointer());
        UTILASSERT(obj);

        obj->SetPosition(glm::vec3(origin.getX(), origin.getZ(), origin.getY()));
        obj->SetRotation(glm::quat(rotation.getW(), rotation.getX(), rotation.getY(), rotation.getZ()));
    }
}

std::optional<Networking_Object> Engine::AddObjectToStatePacket(Object *object, Networking_StatePacket &statePacket, bool includeChildren, bool isRecursive) {
    if (object->GetParent() != nullptr && !isRecursive) {
        return {};
    }

    Networking_Object objectPacket{};

    objectPacket.ObjectID = object->GetObjectID();
    
    objectPacket.position = object->GetPosition(false);
    objectPacket.rotation = object->GetRotation(false);
    objectPacket.scale = object->GetScale(false);

    objectPacket.objectSourceFile = object->GetSourceFile();
    objectPacket.isGeneratedFromFile = object->IsGeneratedFromFile();
    objectPacket.objectSourceID = object->GetSourceID();

    if (object->GetCameraAttachment()) {
        objectPacket.cameraAttachment = object->GetCameraAttachment()->GetCameraID();
    }

    /* Put children before the parent. */
    if (includeChildren) {
        for (Object *child : object->GetChildren()) {
            AddObjectToStatePacket(child, statePacket, true, true);
            objectPacket.children.push_back(child->GetObjectID());
        }
    }

    statePacket.objects.push_back(objectPacket);

    return objectPacket;
}

void Engine::AddObjectToStatePacketIfChanged(Object *object, Networking_StatePacket &statePacket, bool includeChildren, bool isRecursive) {
    auto lastPacketObjectEquivalent = std::find_if(m_LastPacket.objects.begin(), m_LastPacket.objects.end(), [object] (Networking_Object &obj) { return obj.ObjectID == object->GetObjectID(); });
    bool anythingChanged = lastPacketObjectEquivalent == m_LastPacket.objects.end();

    if (!anythingChanged && (
        object->GetPosition(false) != lastPacketObjectEquivalent->position ||
        object->GetRotation(false) != lastPacketObjectEquivalent->rotation ||
        object->GetScale(false) != lastPacketObjectEquivalent->scale ||
        object->IsGeneratedFromFile() != lastPacketObjectEquivalent->isGeneratedFromFile ||
        object->GetSourceFile() != lastPacketObjectEquivalent->objectSourceFile ||
        object->GetSourceID() != lastPacketObjectEquivalent->objectSourceID ||
        object->GetChildren().size() != lastPacketObjectEquivalent->children.size()))
        anythingChanged = true;
    
    if (!anythingChanged && object->GetCameraAttachment() != nullptr && object->GetCameraAttachment()->GetCameraID() != lastPacketObjectEquivalent->cameraAttachment) {
        anythingChanged = true;
    }

    if (includeChildren) {
        for (Object *child : object->GetChildren()) {
            AddObjectToStatePacketIfChanged(child, statePacket, includeChildren, true);
        }
    }

    if (anythingChanged) {
        auto objectPacketOptional = AddObjectToStatePacket(object, statePacket, false, isRecursive);

        if (!objectPacketOptional.has_value()) {
            return;
        }

        Networking_Object objectPacket = objectPacketOptional.value();

        /* Why is there a nullptr check? I don't know!! I don't care!! */
        if (lastPacketObjectEquivalent.base() != nullptr && lastPacketObjectEquivalent != m_LastPacket.objects.end()) {
            *lastPacketObjectEquivalent = objectPacket;
        } else {
            m_LastPacket.objects.push_back(objectPacket);
        }
    }
}

Networking_Camera Engine::AddCameraToStatePacket(Camera *cam, Networking_StatePacket &statePacket, bool isMainCamera) {
    Networking_Camera cameraPacket{};

    cameraPacket.cameraID = cam->GetCameraID();
    cameraPacket.isOrthographic = cam->type == CAMERA_ORTHOGRAPHIC;
    cameraPacket.aspectRatio = cam->AspectRatio;
    cameraPacket.orthographicWidth = cam->OrthographicWidth;
    cameraPacket.pitch = cam->Pitch;
    cameraPacket.yaw = cam->Yaw;
    cameraPacket.up = cam->Up;
    cameraPacket.fov = cam->FOV;
    cameraPacket.isMainCamera = isMainCamera;

    statePacket.cameras.push_back(cameraPacket);

    return cameraPacket;
}

void Engine::AddCameraToStatePacketIfChanged(Camera *cam, Networking_StatePacket &statePacket, bool isMainCamera) {
    auto lastPacketCameraEquivalent = std::find_if(m_LastPacket.cameras.begin(), m_LastPacket.cameras.end(), [cam] (Networking_Camera &cameraPacket) { return cameraPacket.cameraID == cam->GetCameraID(); });
    bool anythingChanged = lastPacketCameraEquivalent == m_LastPacket.cameras.end();

    if (!anythingChanged && (
        (cam->type == CAMERA_ORTHOGRAPHIC) != lastPacketCameraEquivalent->isOrthographic ||
        cam->AspectRatio != lastPacketCameraEquivalent->aspectRatio ||
        cam->OrthographicWidth != lastPacketCameraEquivalent->orthographicWidth ||
        cam->Pitch != lastPacketCameraEquivalent->pitch ||
        cam->Yaw != lastPacketCameraEquivalent->yaw ||
        cam->Up != lastPacketCameraEquivalent->up ||
        cam->FOV != lastPacketCameraEquivalent->fov ||
        /* TODO: This constantly syncs the camera if there's more than one player connected because isMainCamera could change across connections. */
        /* Potential solution: Abstract connections behind a Connection class, and store the last packet for each connection. */
        isMainCamera != lastPacketCameraEquivalent->isMainCamera))
        anythingChanged = true;

    if (anythingChanged) {
        Networking_Camera cameraPacket = AddCameraToStatePacket(cam, statePacket, isMainCamera);

        if (lastPacketCameraEquivalent != m_LastPacket.cameras.end()) {
            m_LastPacket.cameras.at(std::distance(m_LastPacket.cameras.begin(), lastPacketCameraEquivalent)) = cameraPacket;
        } else {
            m_LastPacket.cameras.push_back(cameraPacket);
        }
    }
}

void Engine::SendFullUpdateToConnection(SteamConnection &connection, int tickNumber) {
    Networking_StatePacket statePacket{};
    
    statePacket.tickNumber = tickNumber;

    for (Camera *cam : m_Cameras) {
        bool isMainCamera = false;
        if (m_ConnToCameraAttachment.find(&connection) != m_ConnToCameraAttachment.end() && m_ConnToCameraAttachment[&connection] == cam) {
            isMainCamera = true;
        }

        AddCameraToStatePacket(cam, statePacket, isMainCamera);
    }

    for (Object *object : m_Objects) {
        AddObjectToStatePacket(object, statePacket);
    }

    std::vector<std::byte> serializedPacket;
    
    Serialize(statePacket.tickNumber, serializedPacket);

    Serialize(statePacket.cameras.size(), serializedPacket);

    for (Networking_Camera &cameraPacket : statePacket.cameras) {
        SerializeNetworkingCamera(cameraPacket, serializedPacket);
    }

    Serialize(statePacket.objects.size(), serializedPacket);
    
    for (Networking_Object &objectPacket : statePacket.objects) {
        SerializeNetworkingObject(objectPacket, serializedPacket);
    }

    connection.SendMessage(serializedPacket, k_nSteamNetworkingSend_Reliable);

    m_LastPacket = statePacket;
}


void Engine::SendUpdateToConnection(SteamConnection &connection, int tickNumber) {
    Networking_StatePacket statePacket{};

    statePacket.tickNumber = tickNumber;

    for (Camera *camera : m_Cameras) {
        bool isMainCamera = false;
        if (m_ConnToCameraAttachment.find(&connection) != m_ConnToCameraAttachment.end() && m_ConnToCameraAttachment[&connection] == camera) {
            isMainCamera = true;
        }

        AddCameraToStatePacketIfChanged(camera, statePacket, isMainCamera);
    }
    
    for (Object *object : m_Objects) {
        AddObjectToStatePacketIfChanged(object, statePacket);
    }

    std::vector<std::byte> serializedPacket;

    Serialize(statePacket.tickNumber, serializedPacket);

    Serialize(statePacket.cameras.size(), serializedPacket);

    for (Networking_Camera &cameraPacket : statePacket.cameras) {
        SerializeNetworkingCamera(cameraPacket, serializedPacket);
    }

    Serialize(statePacket.objects.size(), serializedPacket);
    
    for (Networking_Object &objectPacket : statePacket.objects) {
        SerializeNetworkingObject(objectPacket, serializedPacket);
    }

    connection.SendMessage(serializedPacket, k_nSteamNetworkingSend_Reliable);
}

void Engine::SerializeNetworkingObject(Networking_Object &objectPacket, std::vector<std::byte> &dest) {
    Serialize(objectPacket.ObjectID, dest);

    Serialize(objectPacket.position.x, dest);
    Serialize(objectPacket.position.y, dest);
    Serialize(objectPacket.position.z, dest);

    Serialize(objectPacket.rotation.x, dest);
    Serialize(objectPacket.rotation.y, dest);
    Serialize(objectPacket.rotation.z, dest);
    Serialize(objectPacket.rotation.w, dest);

    Serialize(objectPacket.scale.x, dest);
    Serialize(objectPacket.scale.y, dest);
    Serialize(objectPacket.scale.z, dest);

    Serialize(objectPacket.isGeneratedFromFile, dest);

    if (objectPacket.isGeneratedFromFile) {
        Serialize(objectPacket.objectSourceFile, dest);
        Serialize(objectPacket.objectSourceID, dest);
    }

    Serialize(objectPacket.children.size(), dest);

    for (int &childObjectID : objectPacket.children) {
        Serialize(childObjectID, dest);
    }

    Serialize(objectPacket.cameraAttachment, dest);
}

void Engine::SerializeNetworkingCamera(Networking_Camera &cameraPacket, std::vector<std::byte> &dest) {
    Serialize(cameraPacket.cameraID, dest);

    Serialize(cameraPacket.isOrthographic, dest);

    Serialize(cameraPacket.aspectRatio, dest);
    Serialize(cameraPacket.orthographicWidth, dest);

    Serialize(cameraPacket.pitch, dest);
    Serialize(cameraPacket.yaw, dest);

    Serialize(cameraPacket.up.x, dest);
    Serialize(cameraPacket.up.y, dest);

    Serialize(cameraPacket.fov, dest);
    Serialize(cameraPacket.isMainCamera, dest);
}

void Engine::SerializeClientRequest(Networking_ClientRequest &clientRequest, std::vector<std::byte> &dest) {
    Serialize(clientRequest.requestType, dest);

    Serialize(clientRequest.data.size(), dest);

    dest.insert(dest.end(), clientRequest.data.begin(), clientRequest.data.end());
}

void Engine::DeserializeClientRequest(std::vector<std::byte> &serializedClientRequest, Networking_ClientRequest &dest) {
    Deserialize(serializedClientRequest, dest.requestType);

    size_t dataSize = 0;
    Deserialize(serializedClientRequest, dataSize);

    /* Deserialize actually takes away from the vector, so we **should** be able to just copy the rest to data. */
    dest.data = serializedClientRequest;
}

void Engine::FireNetworkEvent(NetworkingEventType type, SteamConnection &conn, std::optional<std::reference_wrapper<std::vector<std::byte>>> data) {
    if (type == EVENT_RECEIVED_CLIENT_REQUEST) {
        UTILASSERT(data.has_value());

        for (auto &listener : m_DataListeners) {
            listener(conn, data.value().get());
        }
        
        return;
    }

    if (m_EventTypeToListenerMap.find(type) != m_EventTypeToListenerMap.end()) {
        for (auto &listener : m_EventTypeToListenerMap[type]) {
            listener(conn);
        }
    }
}

Engine *Engine::m_CallbackInstance = nullptr;
