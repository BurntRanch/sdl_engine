#ifndef ENGINE_HPP
#define ENGINE_HPP

#ifndef VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#define VK_EXT_DEBUG_REPORT_EXTENSION_NAME "VK_EXT_debug_report"
#endif

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

const std::vector<const char *> requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

const std::vector<const char *> requiredInstanceExtensions = { 
};

const std::vector<const char *> requiredLayerExtensions {
    "VK_LAYER_KHRONOS_validation",
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

struct BufferAndMemory {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct ImageAndMemory {
    VkImage image;
    VkDeviceMemory memory;
};

struct TextureBufferAndMemory {
    BufferAndMemory bufferAndMemory;
    Uint32 width;
    Uint32 height;
    Uint8 channels;
};

struct TextureImageAndMemory {
    ImageAndMemory imageAndMemory;
    Uint32 width;
    Uint32 height;
    Uint8 channels;
};

struct UniformBufferObject {
    glm::mat4 viewMatrix;
    glm::mat4 modelMatrix;
    glm::mat4 projectionMatrix;
};

struct RenderModel {
    Model *model;

    std::vector<VkBuffer> vertexBuffers;
    std::vector<Uint32> indices;
    BufferAndMemory indexBuffer;
    TextureImageAndMemory diffTexture;
    VkImageView diffTextureImageView;
    VkSampler diffTextureSampler;

    UniformBufferObject matricesUBO;
    BufferAndMemory matricesUBOBuffer;
    void *matricesUBOMappedMemory;

    VkDescriptorSet descriptorSet;
};

struct RenderPass {
    VkRenderPass vulkanRenderPass;
    PipelineAndLayout graphicsPipeline;
};

class Engine {
public:
    Settings *settings = nullptr;

    ~Engine();

    Model *LoadModel(const string &path);   // this is the first function created to be used by main.cpp

    void  Init();
    void  Start();
private:
    void InitInstance();
    void InitSwapchain();
    void InitFramebuffers(VkRenderPass renderPass, VkImageView depthImageView);
    VkImageView CreateDepthImage();
    PipelineAndLayout CreateGraphicsPipeline(const std::string &shaderName, RenderPass &renderPass, Uint32 subpassIndex, VkFrontFace frontFace, const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts = {});
    VkRenderPass CreateRenderPass(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, size_t subpassCount);
    VkFramebuffer CreateFramebuffer(VkRenderPass renderPass, VkImageView imageView, VkImageView depthImageView);
    bool QuitEventCheck(SDL_Event &event);

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char> &code);
    Uint32 FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties);

    void AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &memory);
    void CopyHostBufferToDeviceBuffer(VkBuffer hostBuffer, VkBuffer deviceBuffer, VkDeviceSize size);

    std::vector<TextureImageAndMemory> LoadTexturesFromMesh(Mesh &mesh);

    BufferAndMemory CreateVertexBuffer(const std::vector<Vertex> &verts);
    BufferAndMemory CreateIndexBuffer(const std::vector<Uint32> &inds);

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    void CopyBufferToImage(TextureBufferAndMemory textureBuffer, VkImage image);
    void ChangeImageLayout(VkImage image, VkFormat format, VkImageLayout oldImageLayout, VkImageLayout newImageLayout);

    TextureBufferAndMemory LoadTextureFromFile(const string_view &name);
    TextureImageAndMemory CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    VkImageView CreateImageView(TextureImageAndMemory &imageAndMemory, VkFormat format, VkImageAspectFlags aspectMask);
    VkSampler CreateSampler(float maxAnisotropy);

    VkFormat FindBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    inline VkFormat FindDepthFormat() {return FindBestFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );};

    inline bool HasStencilAttachment(VkFormat format) {return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;};

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

    VkDescriptorSetLayout m_DescriptorSetLayout = nullptr;
    VkDescriptorPool m_DescriptorPool = nullptr;

    std::vector<RenderModel> m_RenderModels;    // to be used in the loop.

    VkSwapchainKHR m_Swapchain = nullptr;
    std::vector<RenderPass> m_RenderPasses;
    std::vector<PipelineAndLayout> m_PipelineAndLayouts;

    std::vector<VkImage> m_SwapchainImages;
    std::vector<VkImageView> m_SwapchainImageViews;
    std::vector<VkFramebuffer> m_SwapchainFramebuffers;
    Uint32 m_SwapchainImagesCount;
    VkFormat m_SwapchainImageFormat;
    VkExtent2D m_SwapchainExtent;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;

    VkViewport m_Viewport;
    VkRect2D m_Scissor;

    // memory cleanup related, will not include any buffer that is already above (e.g. m_VertexBuffer)
    std::vector<VkImage> m_AllocatedImages;
    std::vector<VkBuffer> m_AllocatedBuffers;
    std::vector<VkDeviceMemory> m_AllocatedMemory;
    std::vector<VkImageView> m_CreatedImageViews;
    std::vector<VkSampler> m_CreatedSamplers;
};

#endif