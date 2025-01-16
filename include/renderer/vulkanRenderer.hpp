#pragma once
#include "renderer/baseRenderer.hpp"
#include <freetype/freetype.h>

class VulkanRenderer;

struct VulkanRendererSharedContext {
    VulkanRenderer *renderer;

    VkDevice engineDevice;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    Settings &settings;

    std::mutex &singleTimeCommandMutex;
};

class VulkanRenderer : public BaseRenderer {
public:
    VulkanRenderer(Settings &settings, Camera *primaryCam) : BaseRenderer(settings, primaryCam) {};
    ~VulkanRenderer() override;
    
    /* Returns a pointer to VulkanRenderer if renderer is a VulkanRenderer, nullptr otherwise. */
    // static VulkanRenderer *Downcast(BaseRenderer *renderer) { return const_cast<VulkanRenderer *>(dynamic_cast<const VulkanRenderer *>(renderer)); };

    virtual inline VulkanRendererSharedContext GetVulkanRendererSharedContext() { return VulkanRendererSharedContext{this, m_EngineDevice, m_EnginePhysicalDevice, m_CommandPool, m_GraphicsQueue, m_Settings, m_SingleTimeCommandMutex}; };

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

    virtual Glyph GenerateGlyph(VulkanRendererSharedContext &sharedContext, FT_Face ftFace, char c, float &x, float &y, float depth);

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

inline Uint32 FindMemoryType(VulkanRendererSharedContext &sharedContext, Uint32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(sharedContext.physicalDevice, &memoryProperties);

    for (Uint32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error(engineError::CANT_FIND_SUITABLE_MEMTYPE);
}

inline void AllocateBuffer(VulkanRendererSharedContext &sharedContext, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(sharedContext.engineDevice, &bufferInfo, NULL, &buffer) != VK_SUCCESS)
        throw std::runtime_error(engineError::CANT_CREATE_VERTEX_BUFFER);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(sharedContext.engineDevice, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(sharedContext, memoryRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(sharedContext.engineDevice, &allocInfo, NULL, &memory) != VK_SUCCESS)
        throw std::runtime_error(engineError::CANT_ALLOCATE_MEMORY);

    vkBindBufferMemory(sharedContext.engineDevice, buffer, memory, 0);
}

inline VkCommandBuffer BeginSingleTimeCommands(VulkanRendererSharedContext &sharedContext) {
    sharedContext.singleTimeCommandMutex.lock();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = sharedContext.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(sharedContext.engineDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

inline void EndSingleTimeCommands(VulkanRendererSharedContext &sharedContext, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(sharedContext.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(sharedContext.graphicsQueue);

    vkFreeCommandBuffers(sharedContext.engineDevice, sharedContext.commandPool, 1, &commandBuffer);

    sharedContext.singleTimeCommandMutex.unlock();
}

inline TextureImageAndMemory CreateImage(VulkanRendererSharedContext &sharedContext, Uint32 width, Uint32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
    TextureImageAndMemory textureImageAndMemory;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(sharedContext.engineDevice, &imageInfo, nullptr, &textureImageAndMemory.imageAndMemory.image) != VK_SUCCESS) {
        throw std::runtime_error(engineError::IMAGE_CREATION_FAILURE);
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(sharedContext.engineDevice, textureImageAndMemory.imageAndMemory.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(sharedContext, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(sharedContext.engineDevice, &allocInfo, nullptr, &textureImageAndMemory.imageAndMemory.memory) != VK_SUCCESS) {
        throw std::runtime_error(engineError::CANT_ALLOCATE_MEMORY);
    }

    textureImageAndMemory.width = width;
    textureImageAndMemory.height = height;
    textureImageAndMemory.channels = getChannelsFromFormats(format);
    textureImageAndMemory.format = format;

    vkBindImageMemory(sharedContext.engineDevice, textureImageAndMemory.imageAndMemory.image, textureImageAndMemory.imageAndMemory.memory, 0);

    return textureImageAndMemory;
}

inline void ChangeImageLayout(VulkanRendererSharedContext &sharedContext, VkImage image, VkFormat format, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(sharedContext);

    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.image = image;
    memoryBarrier.oldLayout = oldImageLayout;
    memoryBarrier.newLayout = newImageLayout;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    memoryBarrier.subresourceRange.baseArrayLayer = 0;
    memoryBarrier.subresourceRange.baseMipLevel = 0;
    memoryBarrier.subresourceRange.layerCount = 1;
    memoryBarrier.subresourceRange.levelCount = 1;

    VkPipelineStageFlags sourceStageFlags;
    VkPipelineStageFlags destStageFlags;

    if (oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        memoryBarrier.srcAccessMask = 0;
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memoryBarrier.srcAccessMask = 0;
        memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }  else if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }  else if (oldImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        throw std::invalid_argument(engineError::UNSUPPORTED_LAYOUT_TRANSITION);
    }

    vkCmdPipelineBarrier(commandBuffer, 
        sourceStageFlags, destStageFlags,
        0, 
        0, nullptr, 
        0, nullptr, 
        1, &memoryBarrier
    );

    EndSingleTimeCommands(sharedContext, commandBuffer);
}

inline void CopyHostBufferToDeviceBuffer(VulkanRendererSharedContext &sharedContext, VkBuffer hostBuffer, VkBuffer deviceBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(sharedContext);

    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;   // start
    bufferCopy.dstOffset = 0;   // also start
    bufferCopy.size = size;

    vkCmdCopyBuffer(commandBuffer, hostBuffer, deviceBuffer, 1, &bufferCopy);

    EndSingleTimeCommands(sharedContext, commandBuffer);
}

inline BufferAndMemory CreateSimpleVertexBuffer(VulkanRendererSharedContext &sharedContext, const std::vector<SimpleVertex> &simpleVerts, bool returnStaging = false) {
    //if (m_VertexBuffer || m_VertexBufferMemory)
    //    throw std::runtime_error(engineError::VERTEX_BUFFER_ALREADY_EXISTS);

    // allocate the staging buffer (this is used for performance, having a buffer readable by the GPU and CPU is slower than a GPU-exclusive memory, so we will use that as our real vertex buffer)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkDeviceSize stagingBufferSize = sizeof(SimpleVertex) * simpleVerts.size();

    AllocateBuffer(sharedContext, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // copy the vertex data into the buffer
    void *data;
    vkMapMemory(sharedContext.engineDevice, stagingBufferMemory, 0, stagingBufferSize, 0, &data);
    SDL_memcpy(data, (void *)simpleVerts.data(), stagingBufferSize);

    if (returnStaging) {
        return {stagingBuffer, stagingBufferMemory, data};
    }

    vkUnmapMemory(sharedContext.engineDevice, stagingBufferMemory);

    // allocate the gpu-exclusive vertex buffer
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    AllocateBuffer(sharedContext, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    CopyHostBufferToDeviceBuffer(sharedContext, stagingBuffer, vertexBuffer, stagingBufferSize);

    vkDestroyBuffer(sharedContext.engineDevice, stagingBuffer, NULL);
    vkFreeMemory(sharedContext.engineDevice, stagingBufferMemory, NULL);

    return {vertexBuffer, vertexBufferMemory};
}

inline BufferAndMemory CreateVertexBuffer(VulkanRendererSharedContext &sharedContext, const std::vector<Vertex> &verts) {
    //if (m_VertexBuffer || m_VertexBufferMemory)
    //    throw std::runtime_error(engineError::VERTEX_BUFFER_ALREADY_EXISTS);

    // allocate the staging buffer (this is used for performance, having a buffer readable by the GPU and CPU is slower than a GPU-exclusive memory, so we will use that as our real vertex buffer)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkDeviceSize stagingBufferSize = sizeof(Vertex) * verts.size();

    AllocateBuffer(sharedContext, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // copy the vertex data into the buffer
    void *data;
    vkMapMemory(sharedContext.engineDevice, stagingBufferMemory, 0, stagingBufferSize, 0, &data);
    SDL_memcpy(data, (void *)verts.data(), stagingBufferSize);
    vkUnmapMemory(sharedContext.engineDevice, stagingBufferMemory);

    // allocate the gpu-exclusive vertex buffer
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    AllocateBuffer(sharedContext, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    CopyHostBufferToDeviceBuffer(sharedContext, stagingBuffer, vertexBuffer, stagingBufferSize);

    vkDestroyBuffer(sharedContext.engineDevice, stagingBuffer, NULL);
    vkFreeMemory(sharedContext.engineDevice, stagingBufferMemory, NULL);

    return {vertexBuffer, vertexBufferMemory};
}

inline BufferAndMemory CreateIndexBuffer(VulkanRendererSharedContext &sharedContext, const std::vector<Uint32> &inds) {
    //if (m_IndexBuffer || m_IndexBufferMemory)
    //    throw std::runtime_error(engineError::INDEX_BUFFER_ALREADY_EXISTS);

    // allocate the staging buffer (this is used for performance, having a buffer readable by the GPU and CPU is slower than a GPU-exclusive memory, so we will use that as our real index buffer)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkDeviceSize stagingBufferSize = sizeof(Uint32) * inds.size();
    AllocateBuffer(sharedContext, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // copy the index data into the buffer
    void *data;
    vkMapMemory(sharedContext.engineDevice, stagingBufferMemory, 0, stagingBufferSize, 0, &data);
    SDL_memcpy(data, (void *)inds.data(), stagingBufferSize);
    vkUnmapMemory(sharedContext.engineDevice, stagingBufferMemory);

    // allocate the gpu-exclusive index buffer
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    AllocateBuffer(sharedContext, stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    CopyHostBufferToDeviceBuffer(sharedContext, stagingBuffer, indexBuffer, stagingBufferSize);

    vkDestroyBuffer(sharedContext.engineDevice, stagingBuffer, NULL);
    vkFreeMemory(sharedContext.engineDevice, stagingBufferMemory, NULL);

    return {indexBuffer, indexBufferMemory};
}

inline void CopyBufferToImage(VulkanRendererSharedContext &sharedContext, TextureBufferAndMemory textureBuffer, VkImage image) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(sharedContext);

    VkBufferImageCopy bufferImageCopy{};
    bufferImageCopy.bufferImageHeight = textureBuffer.height;
    bufferImageCopy.bufferOffset = 0;
    bufferImageCopy.bufferRowLength = 0;

    bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;
    bufferImageCopy.imageSubresource.layerCount = 1;
    bufferImageCopy.imageSubresource.mipLevel = 0;

    bufferImageCopy.imageOffset = {0, 0, 0};
    bufferImageCopy.imageExtent = {
        textureBuffer.width, 
        textureBuffer.height, 
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer, 
        textureBuffer.bufferAndMemory.buffer, 
        image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        1, &bufferImageCopy
    );

    EndSingleTimeCommands(sharedContext, commandBuffer);
}

inline TextureImageAndMemory CreateSinglePixelImage(VulkanRendererSharedContext &sharedContext, glm::vec3 color) {
    /* Allocate the buffer that stores our pixel data. */
    BufferAndMemory textureBufferAndMemory;

    VkDeviceSize bufferSize = sizeof(Uint8) * 4;  // R8G8B8A8

    AllocateBuffer(sharedContext, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, textureBufferAndMemory.buffer, textureBufferAndMemory.memory);

    // copy the texture data into the buffer
    void *data;
    std::array<Uint8, 4> texColors = {static_cast<Uint8>(color.r * 255), static_cast<Uint8>(color.g * 255), static_cast<Uint8>(color.b * 255), 255};

    vkMapMemory(sharedContext.engineDevice, textureBufferAndMemory.memory, 0, bufferSize, 0, &data);
    SDL_memcpy(data, (void *)texColors.data(), bufferSize);
    vkUnmapMemory(sharedContext.engineDevice, textureBufferAndMemory.memory);

    /* Transfer our newly created texture to an image */
    TextureImageAndMemory textureImageAndMemory = CreateImage(sharedContext,
    1, 1,
    VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    ChangeImageLayout(sharedContext, textureImageAndMemory.imageAndMemory.image, 
                VK_FORMAT_R8G8B8A8_SRGB, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
    CopyBufferToImage(sharedContext, {textureBufferAndMemory, 1, 1, 4}, textureImageAndMemory.imageAndMemory.image);
    ChangeImageLayout(sharedContext, textureImageAndMemory.imageAndMemory.image, 
                VK_FORMAT_R8G8B8A8_SRGB, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
    
    vkDestroyBuffer(sharedContext.engineDevice, textureBufferAndMemory.buffer, NULL);
    vkFreeMemory(sharedContext.engineDevice, textureBufferAndMemory.memory, NULL);

    return textureImageAndMemory;
}