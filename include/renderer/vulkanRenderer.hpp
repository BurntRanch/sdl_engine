#pragma once
#include "common.hpp"
#include "renderer/baseRenderer.hpp"
#include <freetype/freetype.h>

class VulkanRenderer : public BaseRenderer {
public:
    VulkanRenderer(Settings &settings, Camera *primaryCam) : BaseRenderer(settings, primaryCam) {};
    ~VulkanRenderer() override;
    
    /* Returns a pointer to VulkanRenderer if renderer is a VulkanRenderer, nullptr otherwise. */
    static VulkanRenderer *Downcast(BaseRenderer *renderer) { return const_cast<VulkanRenderer *>(dynamic_cast<const VulkanRenderer *>(renderer)); };

    void SetMouseCaptureState(bool capturing) override;

    void LoadModel(Model *model) override;   // this is the first function created to be used by main.cpp
    void UnloadModel(Model *model) override;

    void AddUIChildren(UI::GenericElement *element) override;
    bool RemoveUIChildren(UI::GenericElement *element) override;

    /* Find out the type through the "type" member variable and call the appropriate function for you! */
    void AddUIGenericElement(UI::GenericElement *element) override;

    /* Removers return true/false based on if it was found or not found, respectively. */
    bool RemoveUIGenericElement(UI::GenericElement *element) override;

    void AddUIWaypoint(UI::Waypoint *waypoint) override;

    /* Read RemoveUIGenericElement comment */
    bool RemoveUIWaypoint(UI::Waypoint *waypoint) override;

    void AddUIArrows(UI::Arrows *arrows) override;

    /* Read RemoveUIGenericElement comment */
    bool RemoveUIArrows(UI::Arrows *arrows) override;

    void AddUIPanel(UI::Panel *panel) override;

    /* Read RemoveUIGenericElement comment */
    bool RemoveUIPanel(UI::Panel *panel) override;

    void AddUILabel(UI::Label *label) override;

    /* Read RemoveUIGenericElement comment */
    bool RemoveUILabel(UI::Label *label) override;
    
    void SetPrimaryCamera(Camera *cam) override;

    virtual Glyph GenerateGlyph(FT_Face ftFace, char c, float &x, float &y, float depth) override;

    TextureImageAndMemory CreateSinglePixelImage(glm::vec3 color) override;
    
    void AllocateBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferAndMemory &bufferAndMemory) override;

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    TextureImageAndMemory CreateImage(Uint32 width, Uint32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) override;

    void ChangeImageLayout(VkImage image, VkFormat format, VkImageLayout oldImageLayout, VkImageLayout newImageLayout);
    void CopyBufferToImage(TextureBufferAndMemory textureBuffer, ImageAndMemory imageAndMemory) override;

    void DestroyImage(ImageAndMemory imageAndMemory) override;
    
    BufferAndMemory CreateSimpleVertexBuffer(const std::vector<SimpleVertex> &simpleVerts) override;
    BufferAndMemory CreateVertexBuffer(const std::vector<Vertex> &verts) override;
    BufferAndMemory CreateIndexBuffer(const std::vector<Uint32> &inds) override;

    void Init() override;

    void StepRender() override;

protected:
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSet;

    void InitInstance();
    void InitSwapchain();
    void InitFramebuffers(VkRenderPass renderPass, VkImageView depthImageView);
    VkImageView CreateDepthImage(Uint32 width, Uint32 height);
    PipelineAndLayout CreateGraphicsPipeline(const std::string &shaderName, VkRenderPass renderPass, Uint32 subpassIndex, VkFrontFace frontFace, VkViewport viewport, VkRect2D scissor, const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts = {}, bool isSimple = false, bool enableDepth = VK_TRUE);
    VkRenderPass CreateRenderPass(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, size_t subpassCount, VkFormat imageFormat, VkImageLayout initialColorLayout, VkImageLayout finalColorLayout, bool shouldContainDepthImage = true);
    VkFramebuffer CreateFramebuffer(VkRenderPass renderPass, VkImageView imageView, VkExtent2D resolution, VkImageView depthImageView = nullptr);

    void UnloadRenderModel(RenderModel &renderModel) override;

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char> &code);
    Uint32 FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties);

    void CopyHostBufferToDeviceBuffer(VkBuffer hostBuffer, VkBuffer deviceBuffer, VkDeviceSize size);

    RenderModel LoadMesh(Mesh &mesh, Model *model, bool loadTextures = true) override;
    std::array<TextureImageAndMemory, 1> LoadTexturesFromMesh(Mesh &mesh, bool recordAllocations = true) override;

    TextureBufferAndMemory LoadTextureFromFile(const std::string &name) override;
    VkImageView CreateImageView(TextureImageAndMemory &imageAndMemory, VkFormat format, VkImageAspectFlags aspectMask, bool recordCreation = true);
    VkSampler CreateSampler(float maxAnisotropy, bool recordCreation = true);

    VkFormat FindBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    inline VkFormat FindDepthFormat() {return FindBestFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );};

    inline bool HasStencilAttachment(VkFormat format) {return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;};

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

    // memory cleanup related, will not include any buffer that is already above (e.g. m_VertexBuffer)
    std::vector<VkImage> m_AllocatedImages;
    std::vector<VkBuffer> m_AllocatedBuffers;
    std::vector<VkDeviceMemory> m_AllocatedMemory;
    std::vector<VkImageView> m_CreatedImageViews;
    std::vector<VkSampler> m_CreatedSamplers;
};

static Uint8 getChannelsFromFormats(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_SRGB:
            return 1;
        case VK_FORMAT_D32_SFLOAT:
            return 1;
        case VK_FORMAT_R8G8_UINT:
            return 2;
        // unsupported by many hardware, make sure to throw an error.
        // case VK_FORMAT_R8G8B8_UINT:
        //  return 3;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return 4;
        default:
            throw std::runtime_error(engineError::UNSUPPORTED_FORMAT);
    }
}
