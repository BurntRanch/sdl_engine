#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <BulletCollision/BroadphaseCollision/btBroadphaseInterface.h>
#include <BulletCollision/BroadphaseCollision/btDispatcher.h>
#include <BulletCollision/CollisionDispatch/btCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include "camera.hpp"
#include "common.hpp"
#include "isteamnetworkingsockets.h"
#include "steamnetworkingtypes.h"
#include "ui.hpp"
#include "ui/button.hpp"
#include "object.hpp"

#include <LinearMath/btVector3.h>
#include <btBulletDynamicsCommon.h>

#include <future>
#include <mutex>
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
#include "fmt/ranges.h"

#include "error.hpp"
#include "settings.hpp"
#include "model.hpp"

#include <vector>

#define ENGINE_VERSION VK_MAKE_VERSION(0, 0, 1)
#define ENGINE_NAME "BurntEngine Vulkan"

#define ENGINE_FIXED_UPDATERATE 60.0f
#define ENGINE_FIXED_UPDATE_DELTATIME 1.0f/ENGINE_FIXED_UPDATERATE

#define MAX_FRAMES_IN_FLIGHT 2

const std::vector<const char *> requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
};

const std::vector<const char *> requiredInstanceExtensions = { 
};

const std::vector<const char *> requiredLayerExtensions {
#if DEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct PipelineAndLayout {
    VkPipeline pipeline = nullptr;
    VkPipelineLayout layout = nullptr;
};

struct MatricesUBO {
    glm::mat4 viewMatrix;
    glm::mat4 modelMatrix;
    glm::mat4 projectionMatrix;
};

struct UIWaypointUBO {
    glm::vec3 Position;
};

struct UIArrowsUBO {
    glm::vec3 Color;
};

/* Got a weird bug with floats and found out it was because I didn't put alignas(16), I am now extremely paranoid and will put this in every single UBO with a vec2/float. */
struct UIPanelUBO {
alignas(16)    glm::vec4 Dimensions;
alignas(16)    float Depth;
};

struct UILabelPositionUBO {
alignas(16)    glm::vec2 PositionOffset;
alignas(16)    float Depth;
};

struct RenderModel {
    Model *model;

    BufferAndMemory vertexBuffer;

    VkDeviceSize indexBufferSize;
    BufferAndMemory indexBuffer;

    TextureImageAndMemory diffTexture;
    VkImageView diffTextureImageView;
    VkSampler diffTextureSampler;

    glm::vec3 diffColor;

    MatricesUBO matricesUBO;
    BufferAndMemory matricesUBOBuffer;
};

struct RenderUIWaypoint {
    UI::Waypoint *waypoint;

    // might want this later
    // TextureImageAndMemory diffTexture;
    // VkImageView diffTextureImageView;
    // VkSampler diffTextureSampler;

    MatricesUBO matricesUBO;
    BufferAndMemory matricesUBOBuffer;

    UIWaypointUBO waypointUBO;
    BufferAndMemory waypointUBOBuffer;

    VkDescriptorSet descriptorSet;
};

struct RenderUIArrows {
    UI::Arrows *arrows;

    // might want this later
    // TextureImageAndMemory diffTexture;
    // VkImageView diffTextureImageView;
    // VkSampler diffTextureSampler;

    std::array<RenderModel, 3> arrowRenderModels;

    std::array<std::pair<std::pair<MatricesUBO, UIArrowsUBO>, std::pair<BufferAndMemory, BufferAndMemory>>, 3> arrowBuffers;
};

struct RenderUIPanel {
    UI::Panel *panel;

    VkImageView textureView;
    VkSampler textureSampler;

    UIPanelUBO ubo;
    BufferAndMemory uboBuffer;
};

struct RenderUILabel {
    UI::Label *label;

    std::vector<std::pair<char, std::pair<VkImageView, VkSampler>>> textureShaderData;

    UILabelPositionUBO ubo;
    BufferAndMemory uboBuffer;
};

struct RenderPass {
    VkRenderPass vulkanRenderPass;
    PipelineAndLayout graphicsPipeline;
};

class Renderer {
public:
    Renderer(Settings &settings, Camera *primaryCam) : m_PrimaryCamera(primaryCam), m_Settings(settings) {};
    ~Renderer();

    void SetMouseCaptureState(bool capturing);

    void LoadModel(Model *model);   // this is the first function created to be used by main.cpp
    void UnloadModel(Model *model);

    void AddUIChildren(UI::GenericElement *element);
    bool RemoveUIChildren(UI::GenericElement *element);

    /* Find out the type through the "type" member variable and call the appropriate function for you! */
    void AddUIGenericElement(UI::GenericElement *element);

    /* Removers return true/false based on if it was found or not found, respectively. */
    bool RemoveUIGenericElement(UI::GenericElement *element);

    void AddUIWaypoint(UI::Waypoint *waypoint);

    /* Read RemoveUIGenericElement comment */
    bool RemoveUIWaypoint(UI::Waypoint *waypoint);

    void AddUIArrows(UI::Arrows *arrows);

    /* Read RemoveUIGenericElement comment */
    bool RemoveUIArrows(UI::Arrows *arrows);

    void AddUIPanel(UI::Panel *panel);

    /* Read RemoveUIGenericElement comment */
    bool RemoveUIPanel(UI::Panel *panel);

    void AddUILabel(UI::Label *label);

    /* Read RemoveUIGenericElement comment */
    bool RemoveUILabel(UI::Label *label);

    void RegisterUpdateFunction(const std::function<void()> &func);
    // Fixed Updates are called 60 times a second.
    void RegisterFixedUpdateFunction(const std::function<void(std::array<bool, 322>)> &func);

    /* DO NOT 'OR' MULTIPLE EVENT TYPES, REGISTER THE SAME FUNCTION WITH A DIFFERENT TYPE IF YOU WANT THAT. */
    void RegisterSDLEventListener(const std::function<void(SDL_Event *)> &func, SDL_EventType types);

    void SetPrimaryCamera(Camera *cam);

    Glyph GenerateGlyph(EngineSharedContext &sharedContext, FT_Face ftFace, char c, float &x, float &y, float depth);

    inline EngineSharedContext GetSharedContext() { return {this, m_EngineDevice, m_EnginePhysicalDevice, m_CommandPool, m_GraphicsQueue, m_Settings, m_SingleTimeCommandMutex}; };

    void  Init();
    void  Start();
private:
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSet;

    void CallFixedUpdateFunctions(bool *shouldQuitFlag);

    void InitInstance();
    void InitSwapchain();
    void InitFramebuffers(VkRenderPass renderPass, VkImageView depthImageView);
    VkImageView CreateDepthImage(Uint32 width, Uint32 height);
    PipelineAndLayout CreateGraphicsPipeline(const std::string &shaderName, VkRenderPass renderPass, Uint32 subpassIndex, VkFrontFace frontFace, VkViewport viewport, VkRect2D scissor, const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts = {}, bool isSimple = false, bool enableDepth = VK_TRUE);
    VkRenderPass CreateRenderPass(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, size_t subpassCount, VkFormat imageFormat, VkImageLayout initialColorLayout, VkImageLayout finalColorLayout, bool shouldContainDepthImage = true);
    VkFramebuffer CreateFramebuffer(VkRenderPass renderPass, VkImageView imageView, VkExtent2D resolution, VkImageView depthImageView = nullptr);
    bool QuitEventCheck(SDL_Event &event);

    void UnloadRenderModel(RenderModel &renderModel);

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char> &code);
    Uint32 FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties);

    void CopyHostBufferToDeviceBuffer(VkBuffer hostBuffer, VkBuffer deviceBuffer, VkDeviceSize size);

    RenderModel LoadMesh(Mesh &mesh, Model *model, bool loadTextures = true);
    std::array<TextureImageAndMemory, 1> LoadTexturesFromMesh(Mesh &mesh, bool recordAllocations = true);

    TextureBufferAndMemory LoadTextureFromFile(const std::string &name);
    VkImageView CreateImageView(TextureImageAndMemory &imageAndMemory, VkFormat format, VkImageAspectFlags aspectMask, bool recordCreation = true);
    VkSampler CreateSampler(float maxAnisotropy, bool recordCreation = true);

    VkFormat FindBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    inline VkFormat FindDepthFormat() {return FindBestFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );};

    inline bool HasStencilAttachment(VkFormat format) {return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;};

    /* Cameras, high-level stuff. */
    Camera *m_PrimaryCamera;
    Settings &m_Settings;

    std::unordered_map<SDL_EventType, std::vector<std::function<void(SDL_Event *)>>> m_SDLEventListeners;

    std::vector<Glyph> m_GlyphCache;

    std::vector<RenderUIWaypoint> m_RenderUIWaypoints;
    std::vector<RenderUIArrows> m_RenderUIArrows;
    std::vector<RenderUIPanel> m_UIPanels;
    std::vector<RenderUILabel> m_UILabels;

    SDL_Window *m_EngineWindow = nullptr;

    VkDevice m_EngineDevice = nullptr;
    VkSurfaceKHR m_EngineSurface = nullptr;
    VkPhysicalDevice m_EnginePhysicalDevice = nullptr;
    VkInstance m_EngineVulkanInstance = nullptr;

    VkCommandPool m_CommandPool = nullptr;

    VkQueue m_GraphicsQueue = nullptr;
    VkQueue m_PresentQueue = nullptr;
    Uint32 m_GraphicsQueueIndex = UINT32_MAX;
    Uint32 m_PresentQueueIndex = UINT32_MAX;

    std::vector<VkCommandBuffer> m_CommandBuffers;

    VkDescriptorSetLayout m_RenderDescriptorSetLayout = nullptr;
    VkDescriptorSetLayout m_UIWaypointDescriptorSetLayout = nullptr;
    VkDescriptorSetLayout m_UIArrowsDescriptorSetLayout = nullptr;

    VkDescriptorPool m_RenderDescriptorPool = nullptr;
    VkDescriptorPool m_UIWaypointDescriptorPool = nullptr;

    VkDescriptorSet m_RenderDescriptorSet = nullptr;

    VkDescriptorSetLayout m_UIPanelDescriptorSetLayout = nullptr;
    VkDescriptorSetLayout m_UILabelDescriptorSetLayout = nullptr;

    VkDescriptorSetLayout m_RescaleDescriptorSetLayout = nullptr;
    VkDescriptorPool m_RescaleDescriptorPool = nullptr;
    VkDescriptorSet m_RescaleDescriptorSet = nullptr;
    VkSampler m_RescaleRenderSampler = nullptr;

    PipelineAndLayout m_MainGraphicsPipeline; // Used to render the 3D scene
    PipelineAndLayout m_UIWaypointGraphicsPipeline; // Used to add shiny waypoints
    PipelineAndLayout m_UIArrowsGraphicsPipeline; // Used for UI arrows, commonly used in the Map Editor (WIP at the time of writing this comment).
    PipelineAndLayout m_RescaleGraphicsPipeline; // Used to rescale.
    PipelineAndLayout m_UIPanelGraphicsPipeline; // Used for UI Panels.
    PipelineAndLayout m_UILabelGraphicsPipeline; // Used for UI Labels.

    BufferAndMemory m_FullscreenQuadVertexBuffer = {nullptr, nullptr};

    std::vector<RenderModel> m_RenderModels;    // to be used in the loop.

    VkSwapchainKHR m_Swapchain = nullptr;
    std::vector<VkRenderPass> m_RenderPasses;
    std::vector<PipelineAndLayout> m_PipelineAndLayouts;

    VkRenderPass m_MainRenderPass;
    VkFramebuffer m_RenderFramebuffer;
    ImageAndMemory m_RenderImageAndMemory;
    VkFormat m_RenderImageFormat;

    VkRenderPass m_RescaleRenderPass;   // This uses the swapchain framebuffers

    std::vector<VkImage> m_SwapchainImages;
    std::vector<VkImageView> m_SwapchainImageViews;
    std::vector<VkFramebuffer> m_SwapchainFramebuffers;
    Uint32 m_SwapchainImagesCount;
    VkFormat m_SwapchainImageFormat;
    VkExtent2D m_SwapchainExtent;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    std::mutex m_SingleTimeCommandMutex;

    VkViewport m_RenderViewport;
    VkViewport m_DisplayViewport;
    VkRect2D m_RenderScissor;
    VkRect2D m_DisplayScissor;

    // update events
    std::vector<std::function<void()>> m_UpdateFunctions;
    std::vector<std::function<void(std::array<bool, 322>)>> m_FixedUpdateFunctions;

    // keymap, array of 322 booleans, should be indexed by the scancode (e.g. SDL_SCANCODE_UP, SDL_SCANCODE_A), returns whether the key had been pressed.
    std::array<bool, 322> m_KeyMap;

    // memory cleanup related, will not include any buffer that is already above (e.g. m_VertexBuffer)
    std::vector<VkImage> m_AllocatedImages;
    std::vector<VkBuffer> m_AllocatedBuffers;
    std::vector<VkDeviceMemory> m_AllocatedMemory;
    std::vector<VkImageView> m_CreatedImageViews;
    std::vector<VkSampler> m_CreatedSamplers;
};

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
    int cameraID;

    bool isOrthographic = false;
    float aspectRatio;
    float orthographicWidth;
    
    float pitch;
    float yaw;
    glm::vec3 up;
    float fov;
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

    std::vector<std::byte> data;
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

    std::vector<HSteamNetConnection> netConnections;

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
    ~Engine();

    Engine() = default;

    void InitRenderer(Settings &settings, Camera *primaryCamera);

    void InitNetworking();

    /* Initializes the Bullet Physics engine, Relies on Networking being enabled. */
    void InitPhysics();

    /* Stops the physics engine. */
    void DeinitPhysics();

    /* UI::Button listeners will receive events when any button is pressed, along with its ID. */
    /* Due to how it works, this function can be called before the renderer is initialized. */
    void RegisterUIButtonListener(const std::function<void(std::string)> listener);

    /* status is an indicator of which NetworkingThread should accept it, if it's set to NETWORKING_THREAD_ACTIVE_SERVER, it will register to the server, so on. 
     * This handler will be mostly responsible for prediction. */
    void RegisterTickUpdateHandler(const std::function<void(int)> handler, NetworkingThreadStatus status);

    Renderer *GetRenderer();

    void StartRenderer();

    void LoadUIFile(const std::string &name);

    void AddObject(Object *object);

    /* This does not delete the object nor its attachments. */
    void RemoveObject(Object *object);
    void RemoveCamera(Camera *cam);

    bool ImportScene(const std::string &path);
    void ExportScene(const std::string &path);

    void AttachCameraToConnection(Camera *cam, HSteamNetConnection conn);

    void ConnectToGameServer(SteamNetworkingIPAddr ipAddr);
    void HostGameServer(SteamNetworkingIPAddr ipAddr);

    void ConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *callbackInfo);

    void RegisterNetworkEventListener(const std::function<void(HSteamNetConnection)> listener, NetworkingEventType listenerTarget);
    void RegisterNetworkDataListener(const std::function<void(HSteamNetConnection, std::vector<std::byte> &)> listener);

    void DisconnectFromServer();    // Disconnects you from a game server, Safe to call in any situation but wont do anything if you aren't connected to a server.

    void DisconnectClientFromServer(HSteamNetConnection connection);  // Disconnects a client from your server, Call this only if you're hosting a server.

    void StopHostingGameServer();

    void ProcessNetworkEvents(std::vector<Networking_Event> *networkingEvents);

    bool IsConnectedToGameServer();

    /* Sends a packet to the server that gets handled at the program level, this could include inputs n such. */
    void SendRequestToServer(std::vector<std::byte> &data);

    Object *GetObjectByID(int ObjectID);

    UI::GenericElement *GetElementByID(const std::string &id);

    /* Register a button to the Engine, so that it can forward any clicks inside of it to the UIButton Listeners */
    void RegisterUIButton(UI::Button *button);
    void UnregisterUIButton(UI::Button *button);
private:
/*  Systems   */
    Renderer *m_Renderer = nullptr;
    ISteamNetworkingSockets *m_NetworkingSockets;

    std::unique_ptr<btCollisionConfiguration> m_CollisionConfig;
    std::unique_ptr<btDispatcher> m_Dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_Broadphase;
    std::unique_ptr<btConstraintSolver> m_Solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_DynamicsWorld;

    std::vector<std::shared_ptr<btCollisionShape>> m_CollisionShapes;

    /* [0] = client, [1] = server. */
    std::array<NetworkingThreadState, 2> m_NetworkingThreadStates;
    
    HSteamListenSocket m_NetListenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup m_NetPollGroup = k_HSteamNetPollGroup_Invalid;

    std::string m_ScenePath = "";

    std::unordered_map<NetworkingEventType, std::vector<std::function<void(HSteamNetConnection)>>> m_EventTypeToListenerMap;
    std::vector<std::function<void(HSteamNetConnection, std::vector<std::byte> &)>> m_DataListeners;

    std::unordered_map<HSteamNetConnection, Camera *> m_ConnToCameraAttachment;

    std::vector<Networking_Event> m_NetworkingEvents;
    std::mutex m_NetworkingEventsLock;
    
    Settings *m_Settings;
    Camera *m_MainCamera;

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

    void PhysicsStep(float deltaTime);

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
    void SendFullUpdateToConnection(HSteamNetConnection connection, int tickNumber);

    /* Send an update to the client, Keep in mind the server won't send objects that haven't changed to the client. */
    void SendUpdateToConnection(HSteamNetConnection connection, int tickNumber);

    /* Serialize the Networking_Object and append it to dest */
    void SerializeNetworkingObject(Networking_Object &objectPacket, std::vector<std::byte> &dest);

    /* Serialize the Networking_Camera and append it to dest */
    void SerializeNetworkingCamera(Networking_Camera &cameraPacket, std::vector<std::byte> &dest);

    void SerializeClientRequest(Networking_ClientRequest &clientRequest, std::vector<std::byte> &dest);

    void DeserializeClientRequest(std::vector<std::byte> &serializedClientRequest, Networking_ClientRequest &dest);

    void FireNetworkEvent(NetworkingEventType type, HSteamNetConnection conn, std::optional<std::reference_wrapper<std::vector<std::byte>>> data = {});

    void CheckButtonClicks(SDL_Event *event);

    void InitNetworkingThread(NetworkingThreadStatus status);

    void NetworkingThreadClient_Main(NetworkingThreadState &state);
    void NetworkingThreadServer_Main(NetworkingThreadState &state);

    std::vector<std::function<void(std::string)>> m_UIButtonListeners;
    std::vector<UI::Button *> m_UIButtons;
};

#endif
