#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "camera.hpp"
#include "common.hpp"
#include "ui.hpp"
#include "ui/button.hpp"
#include <future>
#include <mutex>
#ifndef VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#define VK_EXT_DEBUG_REPORT_EXTENSION_NAME "VK_EXT_debug_report"
#endif

#include <functional>

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

#include <fmt/core.h>

#include <error.hpp>
#include <settings.hpp>
#include <model.hpp>

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
//   "VK_LAYER_KHRONOS_validation",
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

struct UIPanelUBO {
    glm::vec4 Dimensions;
    float Depth;
};

struct UILabelPositionUBO {
    glm::vec2 PositionOffset;
    float Depth;
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

class Engine {
public:
    Engine(Settings &settings, Camera *primaryCam) : m_PrimaryCamera(primaryCam), m_Settings(settings) {};
    ~Engine();

    void SetMouseCaptureState(bool capturing);

    void LoadModel(Model *model);   // this is the first function created to be used by main.cpp
    void UnloadModel(Model *model);

    // Find out the type through the "type" member variable and call the appropriate function for you!
    void AddUIGenericElement(UI::GenericElement *element);

    void AddUIWaypoint(UI::Waypoint *waypoint);
    void RemoveUIWaypoint(UI::Waypoint *waypoint);

    void AddUIArrows(UI::Arrows *arrows);
    void RemoveUIArrows(UI::Arrows *arrows);

    void AddUIPanel(UI::Panel *panel);
    void RemoveUIPanel(UI::Panel *panel);

    void AddUILabel(UI::Label *label);
    void RemoveUILabel(UI::Label *label);

    void AddUIButton(UI::Button *button);
    void RemoveUIButton(UI::Button *button);

    void RegisterUpdateFunction(const std::function<void()> &func);
    // Fixed Updates are called 60 times a second.
    void RegisterFixedUpdateFunction(const std::function<void(std::array<bool, 322>)> &func);

    void SetPrimaryCamera(Camera &cam);

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

#endif