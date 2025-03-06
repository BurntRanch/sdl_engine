#ifndef _ENGINE_HPP_
#define _ENGINE_HPP_

#include <BulletCollision/BroadphaseCollision/btBroadphaseInterface.h>
#include <BulletCollision/BroadphaseCollision/btDispatcher.h>
#include <BulletCollision/CollisionDispatch/btCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btDynamicsWorld.h>
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "camera.hpp"
#include "common.hpp"
#include "isteamnetworkingsockets.h"
#include "networking/connection.hpp"
#include "steamnetworkingtypes.h"
#include "ui/button.hpp"
#include "object.hpp"

#include <LinearMath/btVector3.h>
#include <btBulletDynamicsCommon.h>

#include <mutex>
#include <thread>
#include <unordered_map>
#ifndef VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#define VK_EXT_DEBUG_REPORT_EXTENSION_NAME "VK_EXT_debug_report"
#endif

#include <functional>

#include <rapidxml.hpp>
#include <rapidxml_print.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "fmt/core.h"

#include "settings.hpp"
#include "model.hpp"

#include <vector>

#define ENGINE_VERSION VK_MAKE_VERSION(0, 0, 1)
#define ENGINE_NAME "BurntEngine Vulkan"

#define ENGINE_FIXED_UPDATERATE 60.0f
#define ENGINE_FIXED_UPDATE_DELTATIME 1.0f/ENGINE_FIXED_UPDATERATE

#define MAX_FRAMES_IN_FLIGHT 2

/* A representation of an Object to be transmitted over the network. */
struct Networking_Object {
    int ObjectID;

    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    bool isGeneratedFromFile;
    std::string objectSourceFile;
    int objectSourceID;

    /* List of objectIDs */
    std::vector<int> children;

    /* index in cameras array */
    int cameraAttachment = -1;
};

struct Networking_Camera {
    int cameraID = 0;

    bool isOrthographic = false;
    float aspectRatio = 0;
    float orthographicWidth = 0;
    
    float pitch = 0;
    float yaw = 0;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float fov = 0.0f;
    bool isMainCamera = false;
};

struct Networking_StatePacket {
    int tickNumber;

    std::vector<Networking_Camera> cameras;
    std::vector<Networking_Object> objects;
};

enum Networking_ClientRequestType {
    CLIENT_REQUEST_DISCONNECT,
    CLIENT_REQUEST_APPLICATION,  /* Application data. */
};

/* in the future, inputs could go here! */
struct Networking_ClientRequest {
    Networking_ClientRequestType requestType;

    std::vector<std::byte> data = {};
};

enum Networking_EventType {
    NETWORKING_NULL,
    NETWORKING_INITIAL_UPDATE,  /* If we just connected to the server */
    NETWORKING_NEW_OBJECT,
    NETWORKING_NEW_CAMERA,
    NETWORKING_UPDATE_OBJECT,
};

/* This isn't meant to be sent over the network, this is meant to be sent between the NetworkThread and the render thread */
struct Networking_Event {
    Networking_EventType type;

    // /* Index of the object, only set if type == NETWORKING_NEW_OBJECT or NETWORKING_UPDATE_OBJECT*/
    // int objectIdx;

    /* The object that's involved in the event, only set if type == NETWORKING_NEW_OBJECT or NETWORKING_UPDATE_OBJECT */
    std::optional<Networking_Object> object;

    /* The camera that's involved in the event, only set if type == NETWORKING_NEW_CAMERA (or, in the future, NETWORKING_UPDATE_CAMERA)*/
    std::optional<Networking_Camera> camera;

    /* if type == NETWORKING_INITIAL_UPDATE, this will be set instead of .object. */
    std::optional<Networking_StatePacket> packet;
};

enum NetworkingThreadStatus {
    NETWORKING_THREAD_INACTIVE = 0,
    NETWORKING_THREAD_ACTIVE_SERVER = 1,
    NETWORKING_THREAD_ACTIVE_CLIENT = 2,
    NETWORKING_THREAD_ACTIVE_BOTH = 3
};

/* I know the name is confusing, this is to be used by applications to listen for specific events like clients disconnecting and such. */
enum NetworkingEventType {
    EVENT_CLIENT_DISCONNECTED,  /* The client disconnected from us */
    EVENT_DISCONNECTED_FROM_SERVER,   /* We (client) disconnected from a remote server */
    EVENT_CLIENT_CONNECTED, /* A new client has connected */
    EVENT_CONNECTED_TO_SERVER, /* We have connected to a server. */

    EVENT_RECEIVED_CLIENT_REQUEST,
};

/* The state stored for every NetworkingThread */
struct NetworkingThreadState {
    int status = NETWORKING_THREAD_INACTIVE;

    std::vector<SteamConnection> connections;

    /* This is dictated by the server. */
    int tickNumber = -1;
    
    /* This is dictated by the client as it predicts the results of future predictions. (prediction is handled by TickUpdate) */
    int predictionTickNumber = -1;
    
    /* if this value is set to -1, then the thread hadn't received a packet yet. */
    int lastSyncedTickNumber = -1;  /* always -1 for the server because the server doesn't really sync with the client, it's a server-authoritative model. */

    std::vector<std::function<void(int)>> tickUpdateHandlers;

    bool shouldQuit = false;
    std::thread thread;
};

class Engine {
public:
    Engine();
    ~Engine();

    void InitRenderer(Settings &settings, Camera *primaryCamera);

    void InitNetworking();

    /* Initializes the Bullet Physics engine. */
    void InitPhysics();

    /* Stops the physics engine. */
    void DeinitPhysics();

    /* UI::Button listeners will receive events when any button is pressed, along with its ID. */
    /* Due to how it works, this function can be called before the renderer is initialized. */
    void RegisterUIButtonListener(const std::function<void(std::string)> &listener);

    /* status is an indicator of which NetworkingThread should accept it, if it's set to NETWORKING_THREAD_ACTIVE_SERVER, it will register to the server, so on. 
     * This handler will be mostly responsible for prediction. */
    void RegisterTickUpdateHandler(const std::function<void(int)> &handler, const NetworkingThreadStatus &status);

    BaseRenderer *GetRenderer();

    void Start();

    void LoadUIFile(const std::string &name);

    void AddObject(Object *object);

    std::vector<Camera *> &GetCameras();

    /* This does not delete the object nor its attachments. */
    void RemoveObject(Object *object);
    void RemoveCamera(Camera *cam);

    bool ImportScene(const std::string &path);
    void ExportScene(const std::string &path);

    void AttachCameraToConnection(Camera *cam, SteamConnection &conn);

    void ConnectToGameServer(SteamNetworkingIPAddr ipAddr);
    void HostGameServer(SteamNetworkingIPAddr ipAddr);

    void ConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *callbackInfo);

    void RegisterNetworkEventListener(const std::function<void(SteamConnection &)> &listener, const NetworkingEventType &listenerTarget);
    void RegisterNetworkDataListener(const std::function<void(SteamConnection &, std::vector<std::byte> &)> &listener);

    void DisconnectFromServer();    // Disconnects you from a game server, Safe to call in any situation but wont do anything if you aren't connected to a server.

    void DisconnectClientFromServer(SteamConnection &connection);  // Disconnects a client from your server, Call this only if you're hosting a server.

    void StopHostingGameServer();

    void ProcessNetworkEvents(std::vector<Networking_Event> *networkingEvents);

    bool IsConnectedToGameServer();

    /* DO NOT 'OR' MULTIPLE EVENT TYPES, REGISTER THE SAME FUNCTION WITH A DIFFERENT TYPE IF YOU WANT THAT. */
    void RegisterSDLEventListener(const std::function<void(SDL_Event *)> &func, SDL_EventType types);

    /* Sends a packet to the server that gets handled at the program level, this could include inputs n such. */
    void SendRequestToServer(std::vector<std::byte> &data);

    Object *GetObjectByID(int ObjectID);

    UI::GenericElement *GetElementByID(const std::string &id);
    
    void RegisterUpdateFunction(const std::function<void()> &func);

    void RegisterFixedUpdateFunction(const std::function<void()> &func);

    /* Register a button to the Engine, so that it can forward any clicks inside of it to the UIButton Listeners */
    void RegisterUIButton(UI::Button *button);
    void UnregisterUIButton(UI::Button *button);
private:
/*  Systems   */
    BaseRenderer *m_Renderer = nullptr;
    ISteamNetworkingSockets *m_NetworkingSockets;

    std::unique_ptr<btDefaultCollisionConfiguration> m_CollisionConfig;
    std::unique_ptr<btCollisionDispatcher> m_Dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_Broadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_Solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_DynamicsWorld;

    /* I don't really like this variable type, but this is basically an array of pointers.
     * We don't own these, they're all owned by the object, but we still can remove/add them from/to the dynamic world.
     */
    std::vector<std::shared_ptr<btRigidBody>> m_RigidBodies;

    /* [0] = client, [1] = server. */
    std::array<NetworkingThreadState, 2> m_NetworkingThreadStates;
    
    HSteamListenSocket m_NetListenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup m_NetPollGroup = k_HSteamNetPollGroup_Invalid;

    std::string m_ScenePath = "";

    std::unordered_map<NetworkingEventType, std::vector<std::function<void(SteamConnection &)>>> m_EventTypeToListenerMap;
    std::vector<std::function<void(SteamConnection &, std::vector<std::byte> &)>> m_DataListeners;

    std::unordered_map<SteamConnection *, Camera *> m_ConnToCameraAttachment;

    std::vector<Networking_Event> m_NetworkingEvents;
    std::mutex m_NetworkingEventsLock;

    std::unordered_map<SDL_EventType, std::vector<std::function<void(SDL_Event *)>>> m_SDLEventToListenerMap;

    // update events
    std::vector<std::function<void()>> m_UpdateFunctions;
    std::vector<std::function<void()>> m_FixedUpdateFunctions;
    
    Settings *m_Settings = nullptr;
    Camera *m_MainCamera = nullptr;

    /* Next 2 variables are for ProcessNetworkEvents */
    /* Objects that were created as a result of ImportFromFile may not have the same ObjectIDs, and will be hard to track. So we store them to compare their SourceIDs */
    std::vector<Networking_Object> m_ObjectsFromImportedObject;

    /* Doesn't include elements that belong in objectsFromImportedObject */
    std::vector<Object *> m_PreviousObjects;

    std::vector<Camera *> m_Cameras;
    std::vector<Object *> m_Objects;
    std::vector<UI::GenericElement *> m_UIElements;

    Networking_StatePacket m_LastPacket;

    /* I'm starting to like this m_CallbackInstance method */

    static Engine *m_CallbackInstance;

    static void onConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *callbackInfo) {
        m_CallbackInstance->ConnectionStatusChanged(callbackInfo);
    }

    bool QuitEventCheck(SDL_Event &event) {
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
            return true;
        return false;
    }

    /* Steps through the physics engine at ENGINE_FIXED_UPDATE_DELTATIME */
    void PhysicsStep();

    /* Do not set isRecursive to true, This is only there to recursively add objs children BEFORE obj. This is a requirement in the protocol.
     * The return is optional (empty if obj is a child and isRecursive == false) but that doesn't mean you have to put it in the statePacket yourself. It's just there incase you want it.
    */
    std::optional<Networking_Object> AddObjectToStatePacket(Object *obj, Networking_StatePacket &statePacket, bool includeChildren = true, bool isRecursive = false);

    /* Do not set isRecursive to true, This is only there to recursively add objs children BEFORE obj. This is a requirement in the protocol. */
    void AddObjectToStatePacketIfChanged(Object *obj, Networking_StatePacket &statePacket, bool includeChildren = true, bool isRecursive = false);

    /* Very similar to the Object equivalent, difference is Cameras don't have children. Make sure isMainCamera is set to true based off of m_ConnToCameraAttachment. */
    Networking_Camera AddCameraToStatePacket(Camera *cam, Networking_StatePacket &statePacket, bool isMainCamera = false);

    /* Very similar to the Object equivalent, difference is Cameras don't have children. Make sure isMainCamera is set to true based off of m_ConnToCameraAttachment. */
    void AddCameraToStatePacketIfChanged(Camera *cam, Networking_StatePacket &statePacket, bool isMainCamera = false);

    /* Deserialization */
    Networking_StatePacket DeserializePacket(std::vector<std::byte> &serializedPacket);

    /* Deserialize the Networking_Object */
    void DeserializeNetworkingObject(std::vector<std::byte> &serializedObjectPacket, Networking_Object &dest);

    /* Deserialize the Networking_Camera */
    void DeserializeNetworkingCamera(std::vector<std::byte> &serializedCameraPacket, Networking_Camera &dest);

    /* Sends a full update to the connection. Sends every single object, regardless whether it has changed, to the client. Avoid sending this unless it's a clients first time connecting. */
    void SendFullUpdateToConnection(SteamConnection &connection, int tickNumber);

    /* Send an update to the client, Keep in mind the server won't send objects that haven't changed to the client. */
    void SendUpdateToConnection(SteamConnection &connection, int tickNumber);

    /* Serialize the Networking_Object and append it to dest */
    void SerializeNetworkingObject(Networking_Object &objectPacket, std::vector<std::byte> &dest);

    /* Serialize the Networking_Camera and append it to dest */
    void SerializeNetworkingCamera(Networking_Camera &cameraPacket, std::vector<std::byte> &dest);

    void SerializeClientRequest(Networking_ClientRequest &clientRequest, std::vector<std::byte> &dest);

    void DeserializeClientRequest(std::vector<std::byte> &serializedClientRequest, Networking_ClientRequest &dest);

    void FireNetworkEvent(NetworkingEventType type, SteamConnection &conn, std::optional<std::reference_wrapper<std::vector<std::byte>>> data = {});

    void CheckButtonClicks(SDL_Event *event);

    void InitNetworkingThread(NetworkingThreadStatus status);

    void NetworkingThreadClient_Main(NetworkingThreadState &state);
    void NetworkingThreadServer_Main(NetworkingThreadState &state);

    std::vector<std::function<void(std::string)>> m_UIButtonListeners;
    std::vector<UI::Button *> m_UIButtons;
};

#endif
