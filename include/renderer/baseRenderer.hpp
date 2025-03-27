#pragma once
#include "Node/Node3D/Model3D/Model3D.hpp"
#include "camera.hpp"
#include "model.hpp"
#include "common.hpp"
#include "settings.hpp"
#include "ui/label.hpp"
#include "ui/panel.hpp"
#include "ui/waypoint.hpp"
#include <SDL3/SDL_events.h>
#include <any>
#include <freetype/freetype.h>
#include <functional>
#include <optional>

class RenderPass;
class GraphicsPipeline;
struct PipelineBinding;
class DescriptorLayout;
class Shader;

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
    const Model3D *model;

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

    UILabelPositionUBO ubo;
    BufferAndMemory uboBuffer;
};

class BaseRenderer {
public:
    BaseRenderer(Settings &settings) : m_Settings(settings) {};
    virtual ~BaseRenderer() {};

    virtual void SetMouseCaptureState(bool capturing) = 0;

    virtual void LoadModel(const Model3D *model) = 0;   // this is the first function created to be used by main.cpp
    virtual void UnloadModel(const Model3D *model) = 0;

    virtual void AddUIChildren(UI::GenericElement *element) = 0;
    virtual bool RemoveUIChildren(UI::GenericElement *element) = 0;

    /* Find out the type through the "type" member variable and call the appropriate function for you! */
    virtual void AddUIGenericElement(UI::GenericElement *element) = 0;
 
    /* Removers return true/false based on if it was found or not found, respectively. */
    virtual bool RemoveUIGenericElement(UI::GenericElement *element) = 0;
 
    virtual void AddUIWaypoint(UI::Waypoint *waypoint) = 0;
 
    /* Read RemoveUIGenericElement comment */
    virtual bool RemoveUIWaypoint(UI::Waypoint *waypoint) = 0;
 
    virtual void AddUIPanel(UI::Panel *panel) = 0;
 
    /* Read RemoveUIGenericElement comment */
    virtual bool RemoveUIPanel(UI::Panel *panel) = 0;
 
    virtual void AddUILabel(UI::Label *label) = 0;
 
    /* Read RemoveUIGenericElement comment */
    virtual bool RemoveUILabel(UI::Label *label) = 0;

    virtual Glyph GenerateGlyph(FT_Face ftFace, char c, float &x, float &y, float depth) = 0;
    
    virtual TextureImageAndMemory CreateSinglePixelImage(glm::vec3 color) = 0;

    /* The "any" object is up to interpretation by the derived Renderer, for VulkanRenderer, this is a pointer to a VkShaderModule_T object. */
    virtual std::any CreateShaderModule(const std::vector<std::byte> &code) = 0;
    virtual void DestroyShaderModule(std::any shaderModule) = 0;

    virtual std::any CreateDescriptorLayout(std::vector<PipelineBinding> &bindings) = 0;

    /* This contains Vulkan flags that can be safely ignored by any renderer not utilizing Vulkan. You may also translate them to your API of choice. */
    virtual void AllocateBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferAndMemory &bufferAndMemory) = 0;

    /* This contains Vulkan flags that can be safely ignored by any renderer not utilizing Vulkan. You may also translate them to your API of choice. */
    virtual TextureImageAndMemory CreateImage(Uint32 width, Uint32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) = 0;
    virtual void CopyBufferToImage(TextureBufferAndMemory textureBuffer, ImageAndMemory imageAndMemory) = 0;

    virtual void DestroyImage(ImageAndMemory imageAndMemory) = 0;

    virtual BufferAndMemory CreateSimpleVertexBuffer(const std::vector<SimpleVertex> &simpleVerts) = 0;
    virtual BufferAndMemory CreateVertexBuffer(const std::vector<Vertex> &verts) = 0;
    virtual BufferAndMemory CreateIndexBuffer(const std::vector<Uint32> &inds) = 0;

    virtual GraphicsPipeline *CreateGraphicsPipeline(const std::vector<Shader> &shaders, RenderPass *renderPass, Uint32 subpassIndex, VkFrontFace frontFace, glm::vec4 viewport, glm::vec4 scissor, const DescriptorLayout &descriptorSetLayout, bool isSimple = VK_TRUE, bool enableDepth = VK_TRUE) = 0;

    virtual void BeginRenderPass(RenderPass *renderPass, std::any framebuffer) = 0;

    virtual void StartNextSubpass() = 0;

    virtual void BeginPipeline(GraphicsPipeline *pipeline) = 0;

    /* vertexCount can typically be set to any arbitrary value ONLY IF INDEX BUFFER IS DEFINED! */
    virtual void Draw(GraphicsPipeline *pipeline, BufferAndMemory vertexBuffer, Uint32 vertexCount = 0, std::optional<BufferAndMemory> indexBuffer = {}, Uint32 indexCount = 0) = 0;

    virtual void EndRenderPass() = 0;

    virtual void Init() = 0;

    /* Render and present the scene once */
    virtual void StepRender() = 0;
friend class Engine;
protected:
    virtual void UnloadRenderModel(RenderModel &renderModel) = 0;

    virtual RenderModel LoadMesh(const Mesh &mesh, const Model3D *model, bool loadTextures = true) = 0;
    virtual std::array<TextureImageAndMemory, 1> LoadTexturesFromMesh(const Mesh &mesh, bool recordAllocations = true) = 0;

    virtual TextureBufferAndMemory LoadTextureFromFile(const std::string &name) = 0;

    /* Cameras, high-level stuff. */
    Settings &m_Settings;

    /* Updated every frame step, this number is always from 0 to MAX_FRAMES_IN_FLIGHT */
    Uint32 m_FrameIndex = 0;

    std::vector<Glyph> m_GlyphCache;

    std::vector<RenderUIWaypoint> m_RenderUIWaypoints;
    std::vector<RenderUIPanel> m_UIPanels;
    std::vector<RenderUILabel> m_UILabels;

    RenderPass *m_MainRenderPass;
    RenderPass *m_RescaleRenderPass;   // This uses the swapchain framebuffers
    
    BufferAndMemory m_FullscreenQuadVertexBuffer = {nullptr, nullptr, 0};
    ImageAndMemory m_RenderImageAndMemory;

    SDL_Window *m_EngineWindow = nullptr;

    std::vector<RenderModel> m_RenderModels;    // to be used in the loop.
};