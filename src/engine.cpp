#include "engine.hpp"

#include "common.hpp"
#include "fmt/base.h"
#include "error.hpp"
#include "fmt/format.h"
#include "isteamnetworkingsockets.h"
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
#include <iterator>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    Uint32 extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

static VkFormat getBestFormatFromChannels(Uint8 channels) {
    switch (channels) {
        case 1:
            return VK_FORMAT_R8_SRGB;
        case 2:
            return VK_FORMAT_R8G8_UINT;
        // unsupported by many hardware, make sure to throw an error.
        // case 3:
        //     return VK_FORMAT_R8G8B8_UINT;
        case 4:
            return VK_FORMAT_R8G8B8A8_SRGB;
        default:
            throw std::runtime_error(engineError::INVALID_CHANNEL_COUNT);
    }
}

static std::vector<char> readFile(const std::string &name) {
    std::ifstream file(name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to read " + name + "!");
    }

    size_t fileSize = file.tellg();
    std::vector<char> output(file.tellg());

    file.seekg(0);
    file.read(output.data(), fileSize);

    return output;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}



/* CLASS IMPLEMENTATIONS AHEAD 
Get ready for loss of braincells!
*/

Renderer::~Renderer() {
    fmt::println("Destroying Engine!");

    if (m_EngineDevice)
        vkDeviceWaitIdle(m_EngineDevice);

    if (m_FullscreenQuadVertexBuffer.buffer && m_FullscreenQuadVertexBuffer.memory) {
        vkDestroyBuffer(m_EngineDevice, m_FullscreenQuadVertexBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, m_FullscreenQuadVertexBuffer.memory, NULL);
    }

    for (RenderModel &renderModel : m_RenderModels)
        this->UnloadModel(renderModel.model);

    for (RenderUIPanel renderPanel : m_UIPanels) {
        this->RemoveUIPanel(renderPanel.panel);

        renderPanel.panel->DestroyBuffers();
    }

    for (RenderUILabel renderLabel : m_UILabels) {
        this->RemoveUILabel(renderLabel.label);

        renderLabel.label->DestroyBuffers();
    }

    for (RenderUIWaypoint &renderUIWaypoint : m_RenderUIWaypoints) {
        this->RemoveUIWaypoint(renderUIWaypoint.waypoint);
    }

    for (RenderUIArrows &renderUIArrows : m_RenderUIArrows) {
        this->RemoveUIArrows(renderUIArrows.arrows);
    }

    for (PipelineAndLayout pipelineAndLayout : m_PipelineAndLayouts) {
        vkDestroyPipeline(m_EngineDevice, pipelineAndLayout.pipeline, NULL);
        vkDestroyPipelineLayout(m_EngineDevice, pipelineAndLayout.layout, NULL);
    }
    for (VkRenderPass renderPass : m_RenderPasses)
        vkDestroyRenderPass(m_EngineDevice, renderPass, NULL);
    for (VkFramebuffer framebuffer : m_SwapchainFramebuffers)
        if (framebuffer)
            vkDestroyFramebuffer(m_EngineDevice, framebuffer, NULL);
    
    if (m_RenderFramebuffer)
        vkDestroyFramebuffer(m_EngineDevice, m_RenderFramebuffer, NULL);

    for (VkImage image : m_AllocatedImages)
        vkDestroyImage(m_EngineDevice, image, NULL);
    for (VkBuffer buffer : m_AllocatedBuffers)
        vkDestroyBuffer(m_EngineDevice, buffer, NULL);
    for (VkDeviceMemory memory : m_AllocatedMemory) {
        //vkUnmapMemory(m_EngineDevice, memory);  // vulkan doesn't care if the memory isn't already mapped, you WILL get a warning from a validation layer tho.
        vkFreeMemory(m_EngineDevice, memory, NULL);
    }
    for (VkImageView imageView : m_CreatedImageViews)
        vkDestroyImageView(m_EngineDevice, imageView, NULL);
    for (VkSampler sampler : m_CreatedSamplers)
        vkDestroySampler(m_EngineDevice, sampler, NULL);
    
    for (VkSemaphore semaphore : m_ImageAvailableSemaphores)
        if (semaphore)
            vkDestroySemaphore(m_EngineDevice, semaphore, NULL);
    for (VkSemaphore semaphore : m_RenderFinishedSemaphores)
        if (semaphore)
            vkDestroySemaphore(m_EngineDevice, semaphore, NULL);
    for (VkFence fence : m_InFlightFences)
        if (fence)
            vkDestroyFence(m_EngineDevice, fence, NULL);

    if (m_Swapchain)
        vkDestroySwapchainKHR(m_EngineDevice, m_Swapchain, NULL);

    for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
        vkDestroyImageView(m_EngineDevice, m_SwapchainImageViews[i], NULL);

    if (m_RenderDescriptorPool)
        vkDestroyDescriptorPool(m_EngineDevice, m_RenderDescriptorPool, NULL);

    if (m_RenderDescriptorSetLayout)
        vkDestroyDescriptorSetLayout(m_EngineDevice, m_RenderDescriptorSetLayout, NULL);

    if (m_UIWaypointDescriptorPool)
        vkDestroyDescriptorPool(m_EngineDevice, m_UIWaypointDescriptorPool, NULL);

    if (m_UIWaypointDescriptorSetLayout)
        vkDestroyDescriptorSetLayout(m_EngineDevice, m_UIWaypointDescriptorSetLayout, NULL);

    if (m_UIArrowsDescriptorSetLayout)
        vkDestroyDescriptorSetLayout(m_EngineDevice, m_UIArrowsDescriptorSetLayout, NULL);

    if (m_UILabelDescriptorSetLayout)
        vkDestroyDescriptorSetLayout(m_EngineDevice, m_UILabelDescriptorSetLayout, NULL);

    if (m_RescaleDescriptorPool)
        vkDestroyDescriptorPool(m_EngineDevice, m_RescaleDescriptorPool, NULL);

    if (m_RescaleDescriptorSetLayout)
        vkDestroyDescriptorSetLayout(m_EngineDevice, m_RescaleDescriptorSetLayout, NULL);

    if (m_UIPanelDescriptorSetLayout)
        vkDestroyDescriptorSetLayout(m_EngineDevice, m_UIPanelDescriptorSetLayout, NULL);

    if (m_CommandPool)
        vkDestroyCommandPool(m_EngineDevice, m_CommandPool, NULL);
    
    if (m_EngineDevice)
        vkDestroyDevice(m_EngineDevice, NULL);
    if (m_EngineSurface)
        vkDestroySurfaceKHR(m_EngineVulkanInstance, m_EngineSurface, NULL);
    if (m_EngineVulkanInstance)
        vkDestroyInstance(m_EngineVulkanInstance, NULL);
    if (m_EngineWindow)
        SDL_DestroyWindow(m_EngineWindow);

    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

VkShaderModule Renderer::CreateShaderModule(VkDevice device, const std::vector<char> &code) {
    VkShaderModuleCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = code.size();
    shaderCreateInfo.pCode = reinterpret_cast<const Uint32*>(code.data());

    VkShaderModule out = nullptr;
    if (vkCreateShaderModule(device, &shaderCreateInfo, NULL, &out) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module!");

    return out;
}

SwapChainSupportDetails Renderer::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities); 

    Uint32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }

    Uint32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void Renderer::CopyHostBufferToDeviceBuffer(VkBuffer hostBuffer, VkBuffer deviceBuffer, VkDeviceSize size) {
    EngineSharedContext sharedContext = GetSharedContext();

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(sharedContext);

    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;   // start
    bufferCopy.dstOffset = 0;   // also start
    bufferCopy.size = size;

    vkCmdCopyBuffer(commandBuffer, hostBuffer, deviceBuffer, 1, &bufferCopy);

    EndSingleTimeCommands(sharedContext, commandBuffer);
}

// first element = diffuse
std::array<TextureImageAndMemory, 1> Renderer::LoadTexturesFromMesh(Mesh &mesh, bool recordAllocations) {
    std::array<TextureImageAndMemory, 1> textures;

    EngineSharedContext sharedContext = GetSharedContext();
    
    {
        if (!mesh.diffuseMapPath.empty()) {
            std::filesystem::path path;
            // map_Kd /home/toni/.../brown_mud_dry_diff_4k.jpg
            if (mesh.diffuseMapPath.has_root_path())
            {
                path = mesh.diffuseMapPath;
            }
            else
            {
                // https://stackoverflow.com/a/73927710
                auto rel = std::filesystem::relative(mesh.diffuseMapPath, "textures");
                // map_Kd textures/brown_mud_dry_diff_4k.jpg
                if (!rel.empty() && rel.native()[0] != '.')
                    path = mesh.diffuseMapPath;
                // map_Kd brown_mud_dry_diff_4k.jpg
                else
                    path = "textures" / mesh.diffuseMapPath;
            }

            TextureBufferAndMemory textureBufferAndMemory = LoadTextureFromFile(path);
            VkFormat textureFormat = getBestFormatFromChannels(textureBufferAndMemory.channels);

            textures[0] = CreateImage(sharedContext,
            textureBufferAndMemory.width, textureBufferAndMemory.height,
            textureFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                );
            ChangeImageLayout(sharedContext, textures[0].imageAndMemory.image, 
                        textureFormat, 
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                    );
            CopyBufferToImage(sharedContext, textureBufferAndMemory, textures[0].imageAndMemory.image);
            ChangeImageLayout(sharedContext, textures[0].imageAndMemory.image, 
                        textureFormat, 
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    );
            
            vkDestroyBuffer(m_EngineDevice, textureBufferAndMemory.bufferAndMemory.buffer, NULL);
            vkFreeMemory(m_EngineDevice, textureBufferAndMemory.bufferAndMemory.memory, NULL);

            m_AllocatedBuffers.erase(std::find(m_AllocatedBuffers.begin(), m_AllocatedBuffers.end(), textureBufferAndMemory.bufferAndMemory.buffer));
            m_AllocatedMemory.erase(std::find(m_AllocatedMemory.begin(), m_AllocatedMemory.end(), textureBufferAndMemory.bufferAndMemory.memory));
        } else {
            textures[0] = CreateSinglePixelImage(sharedContext, mesh.diffuse);
        }
    }

    return textures;
}

TextureBufferAndMemory Renderer::LoadTextureFromFile(const std::string &name) {
    int texWidth, texHeight;
    
    fmt::println("Loading image {} ...", name);
    stbi_uc *imageData = stbi_load(name.data(), &texWidth, &texHeight, nullptr, STBI_rgb_alpha);
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);

    if (!imageData)
    {
        fmt::println("fopen() error: {}", strerror(errno));
        throw std::runtime_error(fmt::format(engineError::TEXTURE_LOADING_FAILURE, stbi_failure_reason(), name));
    }

    fmt::println("Image loaded ({}x{}, {} channels) with an expected buffer size of {}.", texWidth, texHeight, 4, texWidth * texHeight * 4);

    EngineSharedContext sharedContext = GetSharedContext();

    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingMemory;

    AllocateBuffer(sharedContext, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, imageStagingBuffer, imageStagingMemory);
    m_AllocatedBuffers.push_back(imageStagingBuffer);
    m_AllocatedMemory.push_back(imageStagingMemory);

    void *data;
    vkMapMemory(m_EngineDevice, imageStagingMemory, 0, imageSize, 0, &data);
    SDL_memcpy(data, imageData, imageSize);
    vkUnmapMemory(m_EngineDevice, imageStagingMemory);

    stbi_image_free(imageData);

    return {{imageStagingBuffer, imageStagingMemory}, (Uint32)texWidth, (Uint32)texHeight, (Uint8)4};
}

VkImageView Renderer::CreateImageView(TextureImageAndMemory &imageAndMemory, VkFormat format, VkImageAspectFlags aspectMask, bool recordCreation) {
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = imageAndMemory.imageAndMemory.image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_EngineDevice, &imageViewCreateInfo, NULL, &imageView) != VK_SUCCESS)
        throw std::runtime_error(engineError::IMAGE_VIEW_CREATION_FAILURE);

    if (recordCreation) {
        m_CreatedImageViews.push_back(imageView);
    }

    return imageView;
}

VkSampler Renderer::CreateSampler(float maxAnisotropy, bool recordCreation) {
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = maxAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_TRUE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;

    VkSampler sampler;
    if (vkCreateSampler(m_EngineDevice, &samplerCreateInfo, NULL, &sampler) != VK_SUCCESS)
        throw std::runtime_error(engineError::SAMPLER_CREATION_FAILURE);

    if (recordCreation) {
        m_CreatedSamplers.push_back(sampler);
    }

    return sampler;
}

VkFormat Renderer::FindBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_EnginePhysicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error(engineError::CANT_FIND_ANY_FORMAT);
}

RenderModel Renderer::LoadMesh(Mesh &mesh, Model *model, bool loadTextures) {
    EngineSharedContext sharedContext = GetSharedContext();

    RenderModel renderModel{};

    renderModel.model = model;

    BufferAndMemory vertexBuffer = CreateVertexBuffer(sharedContext, mesh.vertices);
    renderModel.vertexBuffer = vertexBuffer;

    renderModel.indexBufferSize = mesh.indices.size();
    renderModel.indexBuffer = CreateIndexBuffer(sharedContext, mesh.indices);

    if (loadTextures) {
        std::array<TextureImageAndMemory, 1> meshTextures = LoadTexturesFromMesh(mesh, false);
        renderModel.diffTexture = meshTextures[0];

        VkFormat textureFormat = getBestFormatFromChannels(renderModel.diffTexture.channels);

        // Image view, for sampling.
        renderModel.diffTextureImageView = CreateImageView(renderModel.diffTexture, textureFormat, VK_IMAGE_ASPECT_COLOR_BIT, false);

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_EnginePhysicalDevice, &properties);

        renderModel.diffTextureSampler = CreateSampler(properties.limits.maxSamplerAnisotropy, false);
    }

    renderModel.diffColor = mesh.diffuse;

    // UBO
    VkDeviceSize uniformBufferSize = sizeof(MatricesUBO);

    renderModel.matricesUBO = {glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};

    AllocateBuffer(sharedContext, uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderModel.matricesUBOBuffer.buffer, renderModel.matricesUBOBuffer.memory);

    vkMapMemory(m_EngineDevice, renderModel.matricesUBOBuffer.memory, 0, uniformBufferSize, 0, &renderModel.matricesUBOBuffer.mappedData);

    return renderModel;
}

void Renderer::SetMouseCaptureState(bool capturing) {
    SDL_SetWindowRelativeMouseMode(m_EngineWindow, capturing);
}

void Renderer::LoadModel(Model *model) {
    std::vector<std::future<RenderModel>> tasks;

    vkDeviceWaitIdle(m_EngineDevice);
    for (Mesh &mesh : model->meshes) {
        tasks.push_back(std::async(std::launch::deferred, &Renderer::LoadMesh, this, std::ref(mesh), model, true));
    }

    // Any exception here is going to just happen and get caught like a regular engine error.
    for (std::future<RenderModel> &task : tasks) {
        m_RenderModels.push_back(task.share().get());
    }

    return;
}

void Renderer::UnloadRenderModel(RenderModel &renderModel) {
    vkDeviceWaitIdle(m_EngineDevice);

    if (renderModel.diffTextureImageView)
        vkDestroyImageView(m_EngineDevice, renderModel.diffTextureImageView, NULL);

    if (renderModel.diffTexture.imageAndMemory.image) {
        vkDestroyImage(m_EngineDevice, renderModel.diffTexture.imageAndMemory.image, NULL);
        vkFreeMemory(m_EngineDevice, renderModel.diffTexture.imageAndMemory.memory, NULL);
    }

    if (renderModel.diffTextureSampler)
        vkDestroySampler(m_EngineDevice, renderModel.diffTextureSampler, NULL);
    
    vkDestroyBuffer(m_EngineDevice, renderModel.indexBuffer.buffer, NULL);
    vkFreeMemory(m_EngineDevice, renderModel.indexBuffer.memory, NULL);

    vkDestroyBuffer(m_EngineDevice, renderModel.vertexBuffer.buffer, NULL);
    vkFreeMemory(m_EngineDevice, renderModel.vertexBuffer.memory, NULL);

    vkDestroyBuffer(m_EngineDevice, renderModel.matricesUBOBuffer.buffer, NULL);
    vkFreeMemory(m_EngineDevice, renderModel.matricesUBOBuffer.memory, NULL);
}

void Renderer::UnloadModel(Model *model) {
    for (size_t i = 0; i < m_RenderModels.size(); i++) {
        if (m_RenderModels[i].model != model)
            continue;

        RenderModel renderModel = m_RenderModels[i];

        m_RenderModels.erase(m_RenderModels.begin() + (i--));

        UnloadRenderModel(renderModel);

        // Since now, the Model object is now owned by the caller.
        // delete model; // delete the pointer to the Model object
    }
}

void Renderer::AddUIChildren(UI::GenericElement *element) {
    for (UI::GenericElement *child : element->GetChildren()) {
        AddUIGenericElement(child);
    }
}

bool Renderer::RemoveUIChildren(UI::GenericElement *element) {
    for (UI::GenericElement *child : element->GetChildren()) {
        RemoveUIGenericElement(child);
    }

    return true;
}

void Renderer::AddUIGenericElement(UI::GenericElement *element) {
    switch (element->type) {
        case UI::PANEL:
            AddUIPanel(reinterpret_cast<UI::Panel *>(element));
            break;
        case UI::LABEL:
            AddUILabel(reinterpret_cast<UI::Label *>(element));
            break;

        /* Button literally just acts like a parent that unifies a Panel & Label, we're adding its children anyway so it doesn't matter. */
        case UI::BUTTON:
        case UI::UNKNOWN:
        case UI::ARROWS:
        case UI::SCALABLE:
        case UI::WAYPOINT:
          break;
        }
    AddUIChildren(element);
}

bool Renderer::RemoveUIGenericElement(UI::GenericElement *element) {
    switch (element->type) {
        case UI::PANEL:
            return RemoveUIPanel(reinterpret_cast<UI::Panel *>(element));
        case UI::LABEL:
            return RemoveUILabel(reinterpret_cast<UI::Label *>(element));

        /* Button literally just acts like a parent that unifies a Panel & Label, we're removing its children anyway so it doesn't matter. */
        case UI::BUTTON:
            return RemoveUIChildren(element);
        case UI::UNKNOWN:
        case UI::ARROWS:
        case UI::SCALABLE:
        case UI::WAYPOINT:
          break;
        }
    
    return RemoveUIChildren(element);
}

void Renderer::AddUIWaypoint(UI::Waypoint *waypoint) {
    EngineSharedContext sharedContext = GetSharedContext();

    RenderUIWaypoint renderUIWaypoint{};

    renderUIWaypoint.waypoint = waypoint;

    // matrices UBO
    VkDeviceSize matricesUniformBufferSize = sizeof(MatricesUBO);

    renderUIWaypoint.matricesUBO = {glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};

    AllocateBuffer(sharedContext, matricesUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderUIWaypoint.matricesUBOBuffer.buffer, renderUIWaypoint.matricesUBOBuffer.memory);

    vkMapMemory(m_EngineDevice, renderUIWaypoint.matricesUBOBuffer.memory, 0, matricesUniformBufferSize, 0, &renderUIWaypoint.matricesUBOBuffer.mappedData);

    // waypoint UBO
    VkDeviceSize waypointUniformBufferSize = sizeof(UIWaypointUBO);

    renderUIWaypoint.waypointUBO = {waypoint->GetWorldSpacePosition()};

    AllocateBuffer(sharedContext, waypointUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderUIWaypoint.waypointUBOBuffer.buffer, renderUIWaypoint.waypointUBOBuffer.memory);

    vkMapMemory(m_EngineDevice, renderUIWaypoint.waypointUBOBuffer.memory, 0, waypointUniformBufferSize, 0, &renderUIWaypoint.waypointUBOBuffer.mappedData);

    std::array<VkDescriptorSetLayout, 1> layouts = { m_UIWaypointDescriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_UIWaypointDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts.data();

    VkResult result = vkAllocateDescriptorSets(m_EngineDevice, &allocInfo, &renderUIWaypoint.descriptorSet);

    if (result != VK_SUCCESS)
        throw std::runtime_error(fmt::format("Failed to allocate descriptor set! ({})", string_VkResult(result)));

    // update descriptor set with buffers
    VkDescriptorBufferInfo matricesBufferInfo{};
    matricesBufferInfo.buffer = renderUIWaypoint.matricesUBOBuffer.buffer;
    matricesBufferInfo.offset = 0;
    matricesBufferInfo.range = matricesUniformBufferSize;

    // update descriptor set with buffer
    VkDescriptorBufferInfo waypointBufferInfo{};
    waypointBufferInfo.buffer = renderUIWaypoint.waypointUBOBuffer.buffer;
    waypointBufferInfo.offset = 0;
    waypointBufferInfo.range = waypointUniformBufferSize;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites;
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].pNext = nullptr;
    descriptorWrites[0].dstSet = renderUIWaypoint.descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &matricesBufferInfo;
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].pNext = nullptr;
    descriptorWrites[1].dstSet = renderUIWaypoint.descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &waypointBufferInfo;

    vkUpdateDescriptorSets(m_EngineDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);

    m_RenderUIWaypoints.push_back(renderUIWaypoint);
}

void Renderer::AddUIArrows(UI::Arrows *arrows) {
    EngineSharedContext sharedContext = GetSharedContext();

    RenderUIArrows renderUIArrows{};

    renderUIArrows.arrows = arrows;

    renderUIArrows.arrowRenderModels[0] = LoadMesh(arrows->arrowsModel->meshes[0], arrows->arrowsModel, false);
    renderUIArrows.arrowRenderModels[1] = LoadMesh(arrows->arrowsModel->meshes[1], arrows->arrowsModel, false);
    renderUIArrows.arrowRenderModels[2] = LoadMesh(arrows->arrowsModel->meshes[2], arrows->arrowsModel, false);

    for (auto &arrowBuffer : renderUIArrows.arrowBuffers) {
        // matrices UBO
        VkDeviceSize matricesUniformBufferSize = sizeof(MatricesUBO);

        arrowBuffer.first.first = {glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};

        AllocateBuffer(sharedContext, matricesUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, arrowBuffer.second.first.buffer, arrowBuffer.second.first.memory);

        vkMapMemory(m_EngineDevice, arrowBuffer.second.first.memory, 0, matricesUniformBufferSize, 0, &arrowBuffer.second.first.mappedData);

        // waypoint UBO
        VkDeviceSize arrowsUniformBufferSize = sizeof(UIArrowsUBO);

        arrowBuffer.first.second = {glm::vec3(1.0f, 1.0f, 1.0f)};

        AllocateBuffer(sharedContext, arrowsUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, arrowBuffer.second.second.buffer, arrowBuffer.second.second.memory);

        vkMapMemory(m_EngineDevice, arrowBuffer.second.second.memory, 0, arrowsUniformBufferSize, 0, &arrowBuffer.second.second.mappedData);
    }

    m_RenderUIArrows.push_back(renderUIArrows);
}

bool Renderer::RemoveUIArrows(UI::Arrows *arrows) {
    RemoveUIChildren(arrows);
    
    bool found = false;

    for (size_t i = 0; i < m_RenderUIArrows.size(); i++) {
        if (m_RenderUIArrows[i].arrows != arrows)
            continue;

        found = true;

        RenderUIArrows renderUIArrows = m_RenderUIArrows[i];

        m_RenderUIArrows.erase(m_RenderUIArrows.begin() + (i--));

        // Before we start, wait for the device to be idle
        // This is already called in ~Engine, but sometimes the user calls RemoveWaypoint manually.
        vkDeviceWaitIdle(m_EngineDevice);

        for (auto &arrowBuffer : renderUIArrows.arrowBuffers) {
            vkDestroyBuffer(m_EngineDevice, arrowBuffer.second.first.buffer, NULL);
            vkFreeMemory(m_EngineDevice, arrowBuffer.second.first.memory, NULL);

            vkDestroyBuffer(m_EngineDevice, arrowBuffer.second.second.buffer, NULL);
            vkFreeMemory(m_EngineDevice, arrowBuffer.second.second.memory, NULL);
        }

        for (RenderModel &renderModel : renderUIArrows.arrowRenderModels) {
            UnloadRenderModel(renderModel);
        }
    }

    return found;
}

bool Renderer::RemoveUIWaypoint(UI::Waypoint *waypoint) {
    RemoveUIChildren(waypoint);

    bool found = false;

    for (size_t i = 0; i < m_RenderUIWaypoints.size(); i++) {
        if (m_RenderUIWaypoints[i].waypoint != waypoint)
            continue;

        found = true;

        RenderUIWaypoint renderUIWaypoint = m_RenderUIWaypoints[i];

        m_RenderUIWaypoints.erase(m_RenderUIWaypoints.begin() + i);

        // Before we start, wait for the device to be idle
        // This is already called in ~Engine, but sometimes the user calls RemoveWaypoint manually.
        vkDeviceWaitIdle(m_EngineDevice);

        // vkDestroyImageView(m_EngineDevice, renderModel.diffTextureImageView, NULL);
        // vkDestroyImage(m_EngineDevice, renderModel.diffTexture.imageAndMemory.image, NULL);
        // vkFreeMemory(m_EngineDevice, renderModel.diffTexture.imageAndMemory.memory, NULL);
        // vkDestroySampler(m_EngineDevice, renderModel.diffTextureSampler, NULL);

        vkDestroyBuffer(m_EngineDevice, renderUIWaypoint.matricesUBOBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, renderUIWaypoint.matricesUBOBuffer.memory, NULL);

        vkDestroyBuffer(m_EngineDevice, renderUIWaypoint.waypointUBOBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, renderUIWaypoint.waypointUBOBuffer.memory, NULL);

        vkFreeDescriptorSets(m_EngineDevice, m_UIWaypointDescriptorPool, 1, &renderUIWaypoint.descriptorSet);
    }

    AddUIChildren(waypoint);

    return found;
}

void Renderer::AddUIPanel(UI::Panel *panel) {
    RenderUIPanel renderUIPanel{};

    renderUIPanel.panel = panel;
    renderUIPanel.textureView = CreateImageView(panel->texture, panel->texture.format, VK_IMAGE_ASPECT_COLOR_BIT, false);
    renderUIPanel.textureSampler = CreateSampler(1.0f, false);

    renderUIPanel.ubo.Dimensions = panel->GetDimensions();
    renderUIPanel.ubo.Depth = panel->GetDepth();

    EngineSharedContext sharedContext = GetSharedContext();

    AllocateBuffer(sharedContext, sizeof(renderUIPanel.ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, renderUIPanel.uboBuffer.buffer, renderUIPanel.uboBuffer.memory);
    vkMapMemory(m_EngineDevice, renderUIPanel.uboBuffer.memory, 0, sizeof(renderUIPanel.ubo), 0, &(renderUIPanel.uboBuffer.mappedData));

    m_UIPanels.push_back(renderUIPanel);
}

bool Renderer::RemoveUIPanel(UI::Panel *panel) {
    RemoveUIChildren(panel);

    bool found = false;

    for (size_t i = 0; i < m_UIPanels.size(); i++) {
        RenderUIPanel renderUIPanel = m_UIPanels[i];

        if (renderUIPanel.panel != panel) {
            continue;
        }

        found = true;

        m_UIPanels.erase(m_UIPanels.begin() + i);

        vkDeviceWaitIdle(m_EngineDevice);

        vkDestroySampler(m_EngineDevice, renderUIPanel.textureSampler, NULL);
        vkDestroyImageView(m_EngineDevice, renderUIPanel.textureView, NULL);

        vkDestroyBuffer(m_EngineDevice, renderUIPanel.uboBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, renderUIPanel.uboBuffer.memory, NULL);

        break;
    }

    return found;
}

void Renderer::AddUILabel(UI::Label *label) {
    EngineSharedContext sharedContext = GetSharedContext();

    RenderUILabel renderUILabel{};

    renderUILabel.label = label;

    for (auto &glyph : label->Glyphs) {
        auto glyphBuffer = glyph.glyphBuffer.value();

        VkImageView textureView = CreateImageView(glyphBuffer.first, glyphBuffer.first.format, VK_IMAGE_ASPECT_COLOR_BIT, false);
        VkSampler textureSampler = CreateSampler(1.0f, false);

        renderUILabel.textureShaderData.push_back(std::make_pair(glyph.character, std::make_pair(textureView, textureSampler)));
    }

    renderUILabel.ubo.PositionOffset = label->GetPosition();
    renderUILabel.ubo.PositionOffset *= 2;

    renderUILabel.ubo.Depth = label->GetDepth();

    AllocateBuffer(sharedContext, sizeof(renderUILabel.ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, renderUILabel.uboBuffer.buffer, renderUILabel.uboBuffer.memory);
    vkMapMemory(m_EngineDevice, renderUILabel.uboBuffer.memory, 0, sizeof(renderUILabel.ubo), 0, &(renderUILabel.uboBuffer.mappedData));

    m_UILabels.push_back(renderUILabel);
}

bool Renderer::RemoveUILabel(UI::Label *label) {
    RemoveUIChildren(label);

    bool found = false;

    for (size_t i = 0; i < m_UILabels.size(); i++) {
        RenderUILabel renderUILabel = m_UILabels[i];

        if (renderUILabel.label != label) {
            continue;
        }

        found = true;

        m_UILabels.erase(m_UILabels.begin() + i);

        vkDeviceWaitIdle(m_EngineDevice);

        for (auto shaderData : renderUILabel.textureShaderData) {
            vkDestroyImageView(m_EngineDevice, shaderData.second.first, NULL);
            vkDestroySampler(m_EngineDevice, shaderData.second.second, NULL);
        }

        renderUILabel.textureShaderData.clear();

        vkDestroyBuffer(m_EngineDevice, renderUILabel.uboBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, renderUILabel.uboBuffer.memory, NULL);

        break;
    }

    return found;
}

void Renderer::RegisterUpdateFunction(const std::function<void()> &func) {
    m_UpdateFunctions.push_back(func);
}

void Renderer::RegisterFixedUpdateFunction(const std::function<void(std::array<bool, 322>)> &func) {
    m_FixedUpdateFunctions.push_back(func);
}

void Renderer::RegisterSDLEventListener(const std::function<void(SDL_Event *)> &func, SDL_EventType types) {
    if (m_SDLEventListeners.find(types) == m_SDLEventListeners.end()) {
        m_SDLEventListeners.insert(std::make_pair(types, std::vector<std::function<void(SDL_Event *)>>()));
    }
    
    m_SDLEventListeners[types].push_back(func);
}

Glyph Renderer::GenerateGlyph(EngineSharedContext &sharedContext, FT_Face ftFace, char c, float &x, float &y, float depth) {
    Glyph glyph{};
    
    glyph.character = c;

    glyph.fontIdentifier = fmt::format("{} {} {}", ftFace->family_name, ftFace->style_name, ftFace->height);

    if (FT_Load_Char(ftFace, c, FT_LOAD_RENDER)) {
        throw std::runtime_error(fmt::format("Failed to load the glyph for '{}' with FreeType", c));
    }

    if (c == ' ') {
        x += ftFace->glyph->advance.x >> 6;

        return glyph;
    }

    if (c == '\n') {
        x = 0;
        y += PIXEL_HEIGHT;

        return glyph;
    }

    for (Glyph &cachedGlyph : m_GlyphCache) {
        if (glyph.character == cachedGlyph.character && 
            glyph.fontIdentifier == cachedGlyph.fontIdentifier) {
                fmt::println("Found an identical glyph in the Glyph Cache!");
                fmt::println("({} from {})", glyph.character, glyph.fontIdentifier);

                glyph = cachedGlyph;

                float xpos = (x + ftFace->glyph->bitmap_left)/static_cast<float>(m_Settings.DisplayWidth);
                float ypos = (y - ftFace->glyph->bitmap_top)/static_cast<float>(m_Settings.DisplayHeight);

                float w = (ftFace->glyph->bitmap.width)/static_cast<float>(m_Settings.DisplayWidth);
                float h = (ftFace->glyph->bitmap.rows)/static_cast<float>(m_Settings.DisplayHeight);

                xpos -= 1.0f;
                ypos -= 1.0f - (PIXEL_HEIGHT_FLOAT / static_cast<float>(m_Settings.DisplayHeight));

                glyph.offset.x = xpos;
                glyph.offset.y = ypos;

                x += ftFace->glyph->advance.x >> 6;

                glyph.scale.x = w;
                glyph.scale.y = h;

                AllocateBuffer(sharedContext, sizeof(glyph.glyphUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyph.glyphUBOBuffer.buffer, glyph.glyphUBOBuffer.memory);
                vkMapMemory(m_EngineDevice, glyph.glyphUBOBuffer.memory, 0, sizeof(glyph.glyphUBO), 0, &(glyph.glyphUBOBuffer.mappedData));

                return glyph;
            }
    }

    VkDeviceSize glyphBufferSize = static_cast<VkDeviceSize>(ftFace->glyph->bitmap.width * ftFace->glyph->bitmap.rows);

    TextureBufferAndMemory glyphBuffer{};
    AllocateBuffer(sharedContext, glyphBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyphBuffer.bufferAndMemory.buffer, glyphBuffer.bufferAndMemory.memory);
    glyphBuffer.width = ftFace->glyph->bitmap.width;
    glyphBuffer.height = ftFace->glyph->bitmap.rows;
    glyphBuffer.channels = 1;

    vkMapMemory(m_EngineDevice, glyphBuffer.bufferAndMemory.memory, 0, glyphBufferSize, 0, &(glyphBuffer.bufferAndMemory.mappedData));
    SDL_memcpy(glyphBuffer.bufferAndMemory.mappedData, ftFace->glyph->bitmap.buffer, glyphBufferSize);

    TextureImageAndMemory textureImageAndMemory = CreateImage(sharedContext, ftFace->glyph->bitmap.width, ftFace->glyph->bitmap.rows, VK_FORMAT_R8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ChangeImageLayout(sharedContext, textureImageAndMemory.imageAndMemory.image, 
                VK_FORMAT_R8_SRGB, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
    CopyBufferToImage(sharedContext, glyphBuffer, textureImageAndMemory.imageAndMemory.image);
    ChangeImageLayout(sharedContext, textureImageAndMemory.imageAndMemory.image, 
                VK_FORMAT_R8_SRGB, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );

    vkDestroyBuffer(m_EngineDevice, glyphBuffer.bufferAndMemory.buffer, NULL);
    vkFreeMemory(m_EngineDevice, glyphBuffer.bufferAndMemory.memory, NULL);

    float xpos = (x + ftFace->glyph->bitmap_left)/static_cast<float>(m_Settings.DisplayWidth);
    float ypos = (y - ftFace->glyph->bitmap_top)/static_cast<float>(m_Settings.DisplayHeight);

    float w = (ftFace->glyph->bitmap.width)/static_cast<float>(m_Settings.DisplayWidth);
    float h = (ftFace->glyph->bitmap.rows)/static_cast<float>(m_Settings.DisplayHeight);

    xpos -= 1.0f;
    ypos -= 1.0f - (PIXEL_HEIGHT_FLOAT / static_cast<float>(m_Settings.DisplayHeight));

    std::vector<SimpleVertex> simpleVerts = {
                                                {glm::vec3(0.0f, 0.0f, depth), glm::vec2(0.0f, 0.0f)},
                                                {glm::vec3(w, h, depth), glm::vec2(1.0f, 1.0f)},
                                                {glm::vec3(0.0f, h, depth), glm::vec2(0.0f, 1.0f)},
                                                {glm::vec3(0.0f, 0.0f, depth), glm::vec2(0.0f, 0.0f)},
                                                {glm::vec3(w, 0.0f, depth), glm::vec2(1.0f, 0.0f)},
                                                {glm::vec3(w, h, depth), glm::vec2(1.0f, 1.0f)}
                                            };

    BufferAndMemory bufferAndMemory = CreateSimpleVertexBuffer(sharedContext, simpleVerts, false);
    
    glyph.offset.x = xpos;
    glyph.offset.y = ypos;

    glyph.scale.x = w;
    glyph.scale.y = h;

    // The bitshift by 6 is required because Advance is 1/64th of a pixel.
    x += ftFace->glyph->advance.x >> 6;

    glyph.glyphBuffer = std::make_pair(textureImageAndMemory, bufferAndMemory);

    AllocateBuffer(sharedContext, sizeof(glyph.glyphUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyph.glyphUBOBuffer.buffer, glyph.glyphUBOBuffer.memory);
    vkMapMemory(m_EngineDevice, glyph.glyphUBOBuffer.memory, 0, sizeof(glyph.glyphUBO), 0, &(glyph.glyphUBOBuffer.mappedData));

    m_GlyphCache.push_back(glyph);

    return glyph;
}

void Renderer::InitSwapchain() {
    if (m_Swapchain) {
        vkDestroySwapchainKHR(m_EngineDevice, m_Swapchain, NULL);
    }

    SwapChainSupportDetails swapchainSupport = QuerySwapChainSupport(m_EnginePhysicalDevice, m_EngineSurface);

    // this will be used to tell the swapchain how many views we want
    Uint32 imageCount = swapchainSupport.capabilities.minImageCount + 1;

    // make sure its valid
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = m_EngineSurface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = swapchainSupport.formats[0].format;
    swapchainCreateInfo.imageColorSpace = swapchainSupport.formats[0].colorSpace;

    m_Settings.DisplayWidth = std::max(swapchainSupport.capabilities.minImageExtent.width, m_Settings.DisplayWidth);
    m_Settings.DisplayHeight = std::max(swapchainSupport.capabilities.minImageExtent.height, m_Settings.DisplayHeight);
    m_Settings.DisplayWidth = std::min(swapchainSupport.capabilities.maxImageExtent.width, m_Settings.DisplayWidth);
    m_Settings.DisplayHeight = std::min(swapchainSupport.capabilities.maxImageExtent.height, m_Settings.DisplayHeight);

    swapchainCreateInfo.imageExtent = {static_cast<Uint32>(m_Settings.DisplayWidth), static_cast<Uint32>(m_Settings.DisplayHeight)}; // WHY???? WHY CAN'T VULKAN JUST TELL ME THIS??

    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Uint32 queueFamilyIndices[] = {m_GraphicsQueueIndex, m_PresentQueueIndex};

    // i think this has to do with exclusive GPU mode where we have exclusive access to the GPU
    if (m_GraphicsQueueIndex != m_PresentQueueIndex) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0; // Optional
        swapchainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    swapchainCreateInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // present mode.
    // this should be one line, but I added support for disabling VSync, or making sure it is on by double-checking that we didn't set it to IMMEDIATE when it shouldn't.
    swapchainCreateInfo.presentMode = swapchainSupport.presentModes[0];
    if (!m_Settings.VSyncEnabled)
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    // make sure we didn't accidentally set VSync off when it should be on.
    else if (m_Settings.VSyncEnabled)
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    // finally create the swapchain
    VkResult swapchainCreateResult = vkCreateSwapchainKHR(m_EngineDevice, &swapchainCreateInfo, nullptr, &m_Swapchain);
    if (swapchainCreateResult != VK_SUCCESS)
        throw std::runtime_error(fmt::format(engineError::SWAPCHAIN_INIT_FAILURE, string_VkResult(swapchainCreateResult)));

    // did we do it? actually???
    SDL_Log("Initialized with errors: %s", SDL_GetError());

    SDL_Log("swapchain == nullptr: %s", m_Swapchain == nullptr ? "true" : "false");

    // swapchain images are basically canvases we can use to draw on, i think.
    vkGetSwapchainImagesKHR(m_EngineDevice, m_Swapchain, &m_SwapchainImagesCount, nullptr);
    m_SwapchainImages.resize(m_SwapchainImagesCount);
    vkGetSwapchainImagesKHR(m_EngineDevice, m_Swapchain, &m_SwapchainImagesCount, m_SwapchainImages.data());

    m_SwapchainImageFormat = swapchainSupport.formats[0].format;
    // m_SwapchainExtent = swapchainSupport.capabilities.currentExtent;
    m_SwapchainExtent = {static_cast<Uint32>(m_Settings.DisplayWidth), static_cast<Uint32>(m_Settings.DisplayHeight)}; // again.

    // we also want to have a VIEW of these images
    // this is like strings vs string_views
    m_SwapchainImageViews.resize(m_SwapchainImagesCount);

    // ofc create the actual image views
    for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = m_SwapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = m_SwapchainImageFormat;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_EngineDevice, &imageViewCreateInfo, NULL, &m_SwapchainImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error(engineError::IMAGE_VIEW_CREATION_FAILURE);
    }
}

void Renderer::InitFramebuffers(VkRenderPass renderPass, VkImageView depthImageView) {
    for (VkFramebuffer framebuffer : m_SwapchainFramebuffers)
        if (framebuffer)
            vkDestroyFramebuffer(m_EngineDevice, framebuffer, NULL);

    m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

    for (size_t i = 0; i < m_SwapchainFramebuffers.size(); i++)
        m_SwapchainFramebuffers[i] = CreateFramebuffer(renderPass, m_SwapchainImageViews[i], {m_Settings.DisplayWidth, m_Settings.DisplayHeight}, depthImageView);
}

VkImageView Renderer::CreateDepthImage(Uint32 width, Uint32 height) {
    EngineSharedContext sharedContext = GetSharedContext();

    VkFormat depthFormat = FindDepthFormat();

    TextureImageAndMemory depthImageAndMemory = CreateImage(sharedContext, width, height, 
                        depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkImageView depthImageView = CreateImageView(depthImageAndMemory, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    m_AllocatedImages.push_back(depthImageAndMemory.imageAndMemory.image);
    m_AllocatedMemory.push_back(depthImageAndMemory.imageAndMemory.memory);

    return depthImageView;
}

Uint32 Renderer::FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_EnginePhysicalDevice, &memoryProperties);

    for (Uint32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error(engineError::CANT_FIND_SUITABLE_MEMTYPE);
}

void Renderer::InitInstance() {
    Uint32 extensionCount;
    // Ask SDL to return a pointer to a C-string array, extensionCount will be set to the length.
    const char * const * instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    if (instanceExtensions == nullptr)
        throw std::runtime_error(engineError::FAILED_VULKAN_EXTS);

    // Merge instanceExtensions and requiredInstanceExtensions.
    std::vector<const char *> extensions;
    
    for (size_t i = 0; i < extensionCount; i++)
        extensions.push_back(instanceExtensions[i]);

    extensions.insert(extensions.end(), requiredInstanceExtensions.begin(), requiredInstanceExtensions.end());

    //const char **extensions = (const char **)SDL_malloc((extensionCount + requiredInstanceExtensions.size()) * sizeof(const char *));
    // SDL_memcpy(&extensions[0], requiredInstanceExtensions.data(), requiredInstanceExtensions.size() * sizeof(const char *));
    // SDL_memcpy(&extensions[requiredInstanceExtensions.size()], instanceExtensions, extensionCount * sizeof(const char *));

    // i think this is how NVIDIA GeForce Experience recognizes compatible games.
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = ENGINE_NAME;
    appInfo.engineVersion = ENGINE_VERSION;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = extensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
    instanceCreateInfo.enabledLayerCount = requiredLayerExtensions.size();
    instanceCreateInfo.ppEnabledLayerNames = requiredLayerExtensions.data();
    
    VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &m_EngineVulkanInstance);
    if (result != VK_SUCCESS)
        throw std::runtime_error(engineError::INSTANCE_CREATION_FAILURE);
}

/* Creates a Vulkan graphics pipeline, shaderName will be used as a part of the path.
 * Sanitization is the job of the caller.
 */
PipelineAndLayout Renderer::CreateGraphicsPipeline(const std::string &shaderName, VkRenderPass renderPass, Uint32 subpassIndex, VkFrontFace frontFace, VkViewport viewport, VkRect2D scissor, const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts, bool isSimple, bool enableDepth) {
    auto vertShader = readFile("shaders/" + shaderName + ".vert.spv");
    auto fragShader = readFile("shaders/" + shaderName + ".frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(m_EngineDevice, vertShader);
    VkShaderModule fragShaderModule = CreateShaderModule(m_EngineDevice, fragShader);

    PipelineAndLayout pipelineAndLayout;

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(m_EngineDevice, &pipelineLayoutInfo, nullptr, &pipelineAndLayout.layout) != VK_SUCCESS)
        throw std::runtime_error(engineError::PIPELINE_LAYOUT_CREATION_FAILURE);

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<Uint32>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;


    auto bindingDescriptionSimple = getSimpleVertexBindingDescription();
    auto attributeDescriptionsSimple = getSimpleVertexAttributeDescriptions();
    auto bindingDescription = getVertexBindingDescription();
    auto attributeDescriptions = getVertexAttributeDescriptions();

    if (isSimple) {
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescriptionSimple;
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptionsSimple.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptionsSimple.data();
    } else {
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = enableDepth;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineAndLayout.layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subpassIndex;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m_EngineDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipelineAndLayout.pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_EngineDevice, vertShaderModule, NULL);
        vkDestroyShaderModule(m_EngineDevice, fragShaderModule, NULL);

        throw std::runtime_error(engineError::PIPELINE_CREATION_FAILURE);
    }

    vkDestroyShaderModule(m_EngineDevice, vertShaderModule, NULL);
    vkDestroyShaderModule(m_EngineDevice, fragShaderModule, NULL);

    m_PipelineAndLayouts.push_back(pipelineAndLayout);

    return pipelineAndLayout;
}

VkRenderPass Renderer::CreateRenderPass(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, size_t subpassCount, VkFormat imageFormat, VkImageLayout initialColorLayout, VkImageLayout finalColorLayout, bool shouldContainDepthImage) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = loadOp;
    colorAttachment.storeOp = storeOp;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = initialColorLayout;
    colorAttachment.finalLayout = finalColorLayout;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = loadOp;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpasses;
    
    for (size_t i = 0; i < subpassCount; i++) {
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (shouldContainDepthImage)
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        subpasses.push_back(subpass);
    }

    std::vector<VkSubpassDependency> subpassDependencies(1);
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDependencies[0].srcAccessMask = 0;

    for (size_t i = 1; i < subpassCount; i++) {
        VkSubpassDependency subpassDependency{};

        for (size_t j = 0; j < i; j++) {
            subpassDependency.srcSubpass = j;
            subpassDependency.dstSubpass = i;
            subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            subpassDependency.srcAccessMask = 0;

            subpassDependencies.push_back(subpassDependency);
        }
    }

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1 + shouldContainDepthImage;
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = subpassCount;
    renderPassInfo.pSubpasses = subpasses.data();

    renderPassInfo.dependencyCount = subpassDependencies.size();
    renderPassInfo.pDependencies = subpassDependencies.data();

    VkRenderPass renderPass;
    if (vkCreateRenderPass(m_EngineDevice, &renderPassInfo, NULL, &renderPass))
        throw std::runtime_error(engineError::RENDERPASS_CREATION_FAILURE);

    m_RenderPasses.push_back(renderPass);

    return renderPass;
}

VkFramebuffer Renderer::CreateFramebuffer(VkRenderPass renderPass, VkImageView imageView, VkExtent2D resolution, VkImageView depthImageView) {
    std::array<VkImageView, 2> attachments = {imageView, depthImageView};

    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = renderPass;
    createInfo.attachmentCount = 1 + (depthImageView != nullptr);   // Bools get converted to ints, This will be 1 if depthImageView == nullptr, 2 if it isn't.
    createInfo.pAttachments = attachments.data();
    createInfo.width = resolution.width;
    createInfo.height = resolution.height;
    createInfo.layers = 1;

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(m_EngineDevice, &createInfo, NULL, &framebuffer) != VK_SUCCESS)
        throw std::runtime_error(engineError::FRAMEBUFFER_CREATION_FAILURE);
    
    return framebuffer;
}


bool Renderer::QuitEventCheck(SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
        return true;
    return false;
}

void Renderer::Init() {
    int SDL_INIT_STATUS = SDL_Init(SDL_INIT_VIDEO);
    if (!SDL_INIT_STATUS)
        throw std::runtime_error(fmt::format(engineError::FAILED_SDL_INIT, SDL_INIT_STATUS));

    m_EngineWindow = SDL_CreateWindow("Test!", m_Settings.DisplayWidth, m_Settings.DisplayHeight, SDL_WINDOW_VULKAN | (m_Settings.Fullscreen & SDL_WINDOW_FULLSCREEN));

    if (!m_EngineWindow)
        throw std::runtime_error(fmt::format(engineError::FAILED_WINDOW_INIT, SDL_GetError()));

    if (m_Settings.Fullscreen && m_Settings.IgnoreRenderResolution) {
        SDL_DisplayMode DM = *SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(m_EngineWindow));
        m_Settings.DisplayWidth = DM.w;
        m_Settings.DisplayHeight = DM.h;

        m_Settings.RenderWidth = DM.w;
        m_Settings.RenderHeight = DM.h;
    } else if (m_Settings.IgnoreRenderResolution) {
        m_Settings.RenderWidth = m_Settings.DisplayWidth;
        m_Settings.RenderHeight = m_Settings.DisplayHeight;
    }

    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "1");

    // lets prepare
    if (!SDL_Vulkan_LoadLibrary(NULL))
        throw std::runtime_error(engineError::FAILED_VULKAN_LOAD);

    // will throw an exception and everything for us
    InitInstance();

    // Get the amount of physical devices, if you set pPhysicalDevices to nullptr it will only dump the length.
    Uint32 physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_EngineVulkanInstance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0)
        throw std::runtime_error(engineError::NO_VULKAN_DEVICES);
    
    if (!SDL_Vulkan_CreateSurface(m_EngineWindow, m_EngineVulkanInstance, NULL, &m_EngineSurface))
        throw std::runtime_error(fmt::format(engineError::SURFACE_CREATION_FAILURE, SDL_GetError()));

    // now we can get a list of physical devices
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(m_EngineVulkanInstance, &physicalDeviceCount, physicalDevices.data());

    // find first capable card, capable cards are cards that have all of the required Device Extensions.
    for (size_t i = 0; i < physicalDevices.size(); i++) {
        if (checkDeviceExtensionSupport(physicalDevices[i])) {
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(physicalDevices[i], &deviceFeatures);

            // does it NOT meet our required device features?
            if (!deviceFeatures.samplerAnisotropy)
                continue;

            // can it work with swapchains? 90% likely, but we still have to check.
            SwapChainSupportDetails swapChainDetails = QuerySwapChainSupport(physicalDevices[i], m_EngineSurface);
            if (swapChainDetails.formats.empty() || swapChainDetails.presentModes.empty())
                continue;

            m_EnginePhysicalDevice = physicalDevices[i];
            break;
        }
    }

    if (!m_EnginePhysicalDevice)
        throw std::runtime_error(engineError::NO_CAPABLE_CARD);
    
    // a Queue Family is a fancy name for a list of lists that store Queues, We only care about the "Graphics Queue Family" and the "Present Queue Family".
    // one is related to handling draw calls, and one is related to handling the presentation of frames.
    Uint32 queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(m_EnginePhysicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
        throw std::runtime_error(engineError::NO_QUEUE_FAMILIES);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_EnginePhysicalDevice, &queueFamilyCount, queueFamilies.data());

    VkBool32 support;
    Uint32 i = 0;

    // find the ones we want.
    for (VkQueueFamilyProperties queueFamily : queueFamilies) {
        if (m_GraphicsQueueIndex == UINT32_MAX && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            m_GraphicsQueueIndex = i;
        if (m_PresentQueueIndex == UINT32_MAX) {
            vkGetPhysicalDeviceSurfaceSupportKHR(m_EnginePhysicalDevice, i, m_EngineSurface, &support);
            if(support)
                m_PresentQueueIndex = i;
        }
        ++i;
    }

    // ask the device we want the graphics queue to be created.
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                    // pNext
        0,                                          // flags
        m_GraphicsQueueIndex,                         // graphicsFamilyIndex
        1,                                          // queueCount
        &queuePriority,                             // pQueuePriorities
    };
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    //const char* deviceExtensionNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,   // sType
        nullptr,                                // pNext
        0,                                      // flags
        1,                       // queueCreateInfoCount
        &queueInfo,                 // pQueueCreateInfos
        0,                          // enabledLayerCount
        nullptr,                  // ppEnabledLayerNames
        (Uint32)requiredDeviceExtensions.size(),         // enabledExtensionCount
        requiredDeviceExtensions.data(),           // ppEnabledExtensionNames
        &deviceFeatures,             // pEnabledFeatures
    };

    if (vkCreateDevice(m_EnginePhysicalDevice, &deviceCreateInfo, nullptr, &m_EngineDevice) != VK_SUCCESS)
        throw std::runtime_error(engineError::CANT_CREATE_DEVICE);

    vkCmdPushDescriptorSet = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetDeviceProcAddr(m_EngineDevice, "vkCmdPushDescriptorSetKHR"));

    vkGetDeviceQueue(m_EngineDevice, m_GraphicsQueueIndex, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_EngineDevice, m_PresentQueueIndex,  0, &m_PresentQueue );

    InitSwapchain();

    m_RenderImageFormat = getBestFormatFromChannels(4);

    m_MainRenderPass = CreateRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 3, m_RenderImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_RescaleRenderPass = CreateRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 3, m_SwapchainImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkImageView depthImageView = CreateDepthImage(m_Settings.RenderWidth, m_Settings.RenderHeight);
    VkImageView rescaleDepthImageView = CreateDepthImage(m_Settings.DisplayWidth, m_Settings.DisplayHeight);

    EngineSharedContext sharedContext = GetSharedContext();
    TextureImageAndMemory renderImage = CreateImage(sharedContext, m_Settings.RenderWidth, m_Settings.RenderHeight, m_RenderImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkImageView renderImageView = CreateImageView(renderImage, m_RenderImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    m_AllocatedImages.push_back(renderImage.imageAndMemory.image);
    m_AllocatedMemory.push_back(renderImage.imageAndMemory.memory);

    // command pools are basically lists of commands, they are recorded and sent to the GPU i think
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueIndex;

    if (vkCreateCommandPool(m_EngineDevice, &commandPoolCreateInfo, NULL, &m_CommandPool) != VK_SUCCESS)
        throw std::runtime_error(engineError::COMMAND_POOL_CREATION_FAILURE);

    // ChangeImageLayout(renderImage.imageAndMemory.image, 
    //             m_RenderImageFormat, 
    //             VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    //         );
    
    m_RenderImageAndMemory = renderImage.imageAndMemory;

    m_RenderFramebuffer = CreateFramebuffer(m_MainRenderPass, renderImageView, {m_Settings.RenderWidth, m_Settings.RenderHeight}, depthImageView);
    InitFramebuffers(m_RescaleRenderPass, rescaleDepthImageView);

    // VkPhysicalDeviceProperties properties{};
    // vkGetPhysicalDeviceProperties(m_EnginePhysicalDevice, &properties);

    // VkSampler sampler = CreateSampler(properties.limits.maxSamplerAnisotropy);

    // struct UniformBufferObject {
    //     glm::mat4 viewMatrix;
    //     glm::mat4 modelMatrix;
    //     glm::mat4 projectionMatrix;
    // } matricesUBO;

    // matricesUBO.viewMatrix = glm::mat4(1.0f);
    // matricesUBO.modelMatrix = glm::mat4(1.0f);
    // matricesUBO.projectionMatrix = glm::mat4(1.0f);
    {
        VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
        samplerDescriptorSetLayoutBinding.binding = 1;
        samplerDescriptorSetLayoutBinding.descriptorCount = 1;
        samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding{};
        uboDescriptorSetLayoutBinding.binding = 0;
        uboDescriptorSetLayoutBinding.descriptorCount = 1;
        uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboDescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &m_RenderDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);
    }

    {
        VkDescriptorSetLayoutBinding matricesUBODescriptorSetLayoutBinding{};
        matricesUBODescriptorSetLayoutBinding.binding = 0;
        matricesUBODescriptorSetLayoutBinding.descriptorCount = 1;
        matricesUBODescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        matricesUBODescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        matricesUBODescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding waypointsUBODescriptorSetLayoutBinding{};
        waypointsUBODescriptorSetLayoutBinding.binding = 1;
        waypointsUBODescriptorSetLayoutBinding.descriptorCount = 1;
        waypointsUBODescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        waypointsUBODescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        waypointsUBODescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {matricesUBODescriptorSetLayoutBinding, waypointsUBODescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &m_UIWaypointDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);
    }
    {
        VkDescriptorSetLayoutBinding arrowInfoDescriptorSetLayoutBinding{};
        arrowInfoDescriptorSetLayoutBinding.binding = 1;
        arrowInfoDescriptorSetLayoutBinding.descriptorCount = 1;
        arrowInfoDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        arrowInfoDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        arrowInfoDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding{};
        uboDescriptorSetLayoutBinding.binding = 0;
        uboDescriptorSetLayoutBinding.descriptorCount = 1;
        uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboDescriptorSetLayoutBinding, arrowInfoDescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &m_UIArrowsDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);
    }

    {
        VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
        samplerDescriptorSetLayoutBinding.binding = 0;
        samplerDescriptorSetLayoutBinding.descriptorCount = 1;
        samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 1> bindings = {samplerDescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &m_RescaleDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);
    }

    {
        VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
        samplerDescriptorSetLayoutBinding.binding = 1;
        samplerDescriptorSetLayoutBinding.descriptorCount = 1;
        samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding{};
        uboDescriptorSetLayoutBinding.binding = 0;
        uboDescriptorSetLayoutBinding.descriptorCount = 1;
        uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {samplerDescriptorSetLayoutBinding, uboDescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &m_UIPanelDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);
    }

    {
        VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
        samplerDescriptorSetLayoutBinding.binding = 1;
        samplerDescriptorSetLayoutBinding.descriptorCount = 1;
        samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding labelUBODescriptorSetLayoutBinding{};
        labelUBODescriptorSetLayoutBinding.binding = 0;
        labelUBODescriptorSetLayoutBinding.descriptorCount = 1;
        labelUBODescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        labelUBODescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        labelUBODescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding glyphUBODescriptorSetLayoutBinding{};
        glyphUBODescriptorSetLayoutBinding.binding = 2;
        glyphUBODescriptorSetLayoutBinding.descriptorCount = 1;
        glyphUBODescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        glyphUBODescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        glyphUBODescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {labelUBODescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding, glyphUBODescriptorSetLayoutBinding};

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &m_UILabelDescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);
    }

    // Render
    m_RenderViewport.x = 0.0f;
    m_RenderViewport.y = 0.0f;
    m_RenderViewport.width = (float) m_Settings.RenderWidth;
    m_RenderViewport.height = (float) m_Settings.RenderHeight;
    m_RenderViewport.minDepth = 0.0f;
    m_RenderViewport.maxDepth = 1.0f;

    m_RenderScissor.offset = {0, 0};
    m_RenderScissor.extent = {m_Settings.RenderWidth, m_Settings.RenderHeight};


    // Rescale
    m_DisplayViewport.x = 0.0f;
    m_DisplayViewport.y = 0.0f;
    m_DisplayViewport.width = (float) m_Settings.DisplayWidth;
    m_DisplayViewport.height = (float) m_Settings.DisplayHeight;
    m_DisplayViewport.minDepth = 0.0f;
    m_DisplayViewport.maxDepth = 1.0f;

    m_DisplayScissor.offset = {0, 0};
    m_DisplayScissor.extent = {m_Settings.DisplayWidth, m_Settings.DisplayHeight};

    m_MainGraphicsPipeline = CreateGraphicsPipeline("lighting", m_MainRenderPass, 0, VK_FRONT_FACE_COUNTER_CLOCKWISE, m_RenderViewport, m_RenderScissor, {m_RenderDescriptorSetLayout});
    m_UIWaypointGraphicsPipeline = CreateGraphicsPipeline("uiwaypoint", m_MainRenderPass, 1, VK_FRONT_FACE_CLOCKWISE, m_RenderViewport, m_RenderScissor, {m_UIWaypointDescriptorSetLayout}, true);
    m_UIArrowsGraphicsPipeline = CreateGraphicsPipeline("uiarrows", m_MainRenderPass, 2, VK_FRONT_FACE_CLOCKWISE, m_RenderViewport, m_RenderScissor, {m_UIArrowsDescriptorSetLayout}, false, VK_FALSE);
    m_RescaleGraphicsPipeline = CreateGraphicsPipeline("rescale", m_RescaleRenderPass, 0, VK_FRONT_FACE_CLOCKWISE, m_DisplayViewport, m_DisplayScissor, {m_RescaleDescriptorSetLayout}, true);
    m_UIPanelGraphicsPipeline = CreateGraphicsPipeline("uipanel", m_RescaleRenderPass, 1, VK_FRONT_FACE_CLOCKWISE, m_DisplayViewport, m_DisplayScissor, {m_UIPanelDescriptorSetLayout}, true);
    m_UILabelGraphicsPipeline = CreateGraphicsPipeline("uilabel", m_RescaleRenderPass, 2, VK_FRONT_FACE_CLOCKWISE, m_DisplayViewport, m_DisplayScissor, {m_UILabelDescriptorSetLayout}, true);

    /* RENDER DESCRIPTOR POOL INITIALIZATION */
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT};

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
        descriptorPoolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(m_EngineDevice, &descriptorPoolCreateInfo, NULL, &m_RenderDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool!");
    }
    
    /* UI WAYPOINT DESCRIPTOR POOL INITIALIZATION */
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT,
                                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT};

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
        descriptorPoolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(m_EngineDevice, &descriptorPoolCreateInfo, NULL, &m_UIWaypointDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool!");
    }
    
    /* RESCALE DESCRIPTOR POOL INITIALIZATION */
    {
        std::array<VkDescriptorPoolSize, 1> poolSizes = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT};

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
        descriptorPoolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(m_EngineDevice, &descriptorPoolCreateInfo, NULL, &m_RescaleDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool!");
    }

    // Image view, for sampling the render texture for rescaling.
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_EnginePhysicalDevice, &properties);

    m_RescaleRenderSampler = CreateSampler(properties.limits.maxSamplerAnisotropy);

    {
        std::vector<VkDescriptorSetLayout> layouts(1, m_RescaleDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_RescaleDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_EngineDevice, &allocInfo, &m_RescaleDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor set for rescale render pass!");

        // update descriptor set with image
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = renderImageView;
        imageInfo.sampler = m_RescaleRenderSampler;

        std::array<VkWriteDescriptorSet, 1> descriptorWrites;
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].pNext = nullptr;
        descriptorWrites[0].dstSet = m_RescaleDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_EngineDevice, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        
        EngineSharedContext sharedContext = GetSharedContext();
        
        // Fullscreen Quad initialization
        m_FullscreenQuadVertexBuffer = CreateSimpleVertexBuffer(sharedContext, {
                                                            {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
                                                            {glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},
                                                            {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
                                                            {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
                                                            {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)}
                                                        });
    }

    // {
    //     std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_RenderDescriptorSetLayout);
    //     VkDescriptorSetAllocateInfo allocInfo = {};
    //     allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    //     allocInfo.descriptorPool = m_RenderDescriptorPool;
    //     allocInfo.descriptorSetCount = 1;
    //     allocInfo.pSetLayouts = layouts.data();

    //     if (vkAllocateDescriptorSets(m_EngineDevice, &allocInfo, &m_RenderDescriptorSet) != VK_SUCCESS)
    //         throw std::runtime_error("Failed to allocate descriptor set!");
    // }

    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    // and this is an individual list.
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandBufferCount = m_CommandBuffers.size();
    commandBufferAllocateInfo.commandPool = m_CommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if (vkAllocateCommandBuffers(m_EngineDevice, &commandBufferAllocateInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error(engineError::COMMAND_BUFFER_ALLOCATION_FAILURE);

    // sync objects, to maintain order in rendering.
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        if (vkCreateSemaphore(m_EngineDevice, &semaphoreCreateInfo, NULL, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_EngineDevice, &semaphoreCreateInfo, NULL, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_EngineDevice, &fenceCreateInfo,     NULL,               &m_InFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error(engineError::SYNC_OBJECTS_CREATION_FAILURE);

    std::fill(m_KeyMap.begin(), m_KeyMap.end(), false);
}

void Renderer::CallFixedUpdateFunctions(bool *shouldQuitFlag) {
    using namespace std::chrono;
    using namespace std::this_thread;

    while (!(*shouldQuitFlag)) {
        sleep_for(milliseconds((int)(ENGINE_FIXED_UPDATE_DELTATIME*1000)));
        for (auto &fixedUpdateFunction : m_FixedUpdateFunctions)
            fixedUpdateFunction(m_KeyMap);
    }
}

void Renderer::Start() {
    // NOW, we are getting to the while loop.
    bool shouldQuit = false;
    Uint32 currentFrameIndex = 0;

    using namespace std::chrono;

    high_resolution_clock::time_point lastFrameTime, frameTime;
    high_resolution_clock::time_point afterEventsTime, afterFenceTime, afterAcquireImageResultTime, afterUpdateTime, afterRenderTime, postRenderTime;

    double accumulative = 0.0;

    lastFrameTime = high_resolution_clock::now();

    while (!shouldQuit) {
        frameTime = high_resolution_clock::now();

        double deltaTime = (duration_cast<duration<double>>(frameTime - lastFrameTime).count());

        if (m_Settings.ReportFPS) {
            fmt::println("{:.0f} FPS, Render Time: {:.5f}s", 1.0/deltaTime, deltaTime);
        }

        lastFrameTime = frameTime;

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (QuitEventCheck(event))
                shouldQuit = true;
            
            switch (event.type) {
                case SDL_EVENT_KEY_DOWN:
                    m_KeyMap[event.key.scancode] = true;
                    break;
                case SDL_EVENT_KEY_UP:
                    m_KeyMap[event.key.scancode] = false;
                    break;
            }

            try {
                auto &listeners = m_SDLEventListeners.at((SDL_EventType)(event.type));

                for (auto &listener : listeners) {
                    listener(&event);
                }
            } catch (const std::out_of_range &e) {
                continue;
            }
        }

#ifdef LOG_FRAME
            afterEventsTime = high_resolution_clock::now();

            fmt::println("Time spent processing events: {:.5f}ms", (duration_cast<duration<double, std::milli>>(afterEventsTime - frameTime).count()));
#endif

        // we got (MAX_FRAMES_IN_FLIGHT) "slots" to use, we can write frames as long as the current frame slot we're using isn't occupied.
        VkResult waitForFencesResult;

        while (waitForFencesResult != VK_SUCCESS) {
            /* 10ms wait */
            waitForFencesResult = vkWaitForFences(m_EngineDevice, 1, &m_InFlightFences[currentFrameIndex], true, 10000000);
            if (waitForFencesResult != VK_TIMEOUT) {
                throw std::runtime_error(fmt::format(engineError::WAIT_FOR_FENCES_FAILED, string_VkResult(waitForFencesResult)));
            }
        }

#ifdef LOG_FRAME
        afterFenceTime = high_resolution_clock::now();

        fmt::println("Time spent waiting for fences: {:.5f}ms", (duration_cast<duration<double, std::milli>>(afterFenceTime - afterEventsTime).count()));
#endif

        Uint32 imageIndex = 0;
        VkResult acquireNextImageResult = vkAcquireNextImageKHR(m_EngineDevice, m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[currentFrameIndex], VK_NULL_HANDLE, &imageIndex);
        
        // the swapchain can become "out of date" if the user were to, say, resize the window.
        // suboptimal means it is kind of out of date but not invalid, can still be used.
        if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
            InitSwapchain();


            VkImageView rescaleDepthImageView = CreateDepthImage(m_Settings.DisplayWidth, m_Settings.DisplayHeight);

            // VkImageView depthImageView = CreateDepthImage();

            // TextureImageAndMemory renderImage = CreateImage(m_Settings.RenderWidth, m_Settings.RenderHeight, m_RenderImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            // VkImageView renderImageView = CreateImageView(renderImage, m_RenderImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

            // m_RenderFramebuffer = CreateFramebuffer(m_MainRenderPass, renderImageView, {m_Settings.RenderWidth, m_Settings.RenderHeight}, depthImageView);
            InitFramebuffers(m_RescaleRenderPass, rescaleDepthImageView);

            continue;
        } else if (acquireNextImageResult != VK_SUCCESS)
            throw std::runtime_error(engineError::CANT_ACQUIRE_NEXT_IMAGE);

#ifdef LOG_FRAME
        afterAcquireImageResultTime = high_resolution_clock::now();

        fmt::println("Time spent acquiring image result: {:.5f}ms", (duration_cast<duration<double, std::milli>>(afterAcquireImageResultTime - afterFenceTime).count()));
#endif

        // Fixed updates
        accumulative += deltaTime;

        // while loop, we are compensating for each fixed update frame potentially missed.
        while (accumulative >= ENGINE_FIXED_UPDATE_DELTATIME) {
            for (auto &fixedUpdateFunction : m_FixedUpdateFunctions) {
                fixedUpdateFunction(m_KeyMap);
            }

            accumulative -= ENGINE_FIXED_UPDATE_DELTATIME;
        }

        for (auto &updateFunction : m_UpdateFunctions)
            updateFunction();

        #ifdef LOG_FRAME
            afterUpdateTime = high_resolution_clock::now();

            fmt::println("Time spent calling update functions: {:.5f}ms", (duration_cast<duration<double, std::milli>>(afterUpdateTime - afterAcquireImageResultTime).count()));
        #endif

        // "ight im available, if i wasn't already"
        vkResetFences(m_EngineDevice, 1, &m_InFlightFences[currentFrameIndex]);

        vkResetCommandBuffer(m_CommandBuffers[currentFrameIndex], 0);

        // begin recording my commands
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(m_CommandBuffers[currentFrameIndex], &commandBufferBeginInfo) != VK_SUCCESS)
            throw std::runtime_error(engineError::COMMAND_BUFFER_BEGIN_FAILURE);

        {
            // begin a render pass, this could be for example HDR pass, SSAO pass, lighting pass.
            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = m_MainRenderPass;
            renderPassBeginInfo.framebuffer = m_RenderFramebuffer;
            renderPassBeginInfo.renderArea.offset = {0, 0};
            renderPassBeginInfo.renderArea.extent = {m_Settings.RenderWidth, m_Settings.RenderHeight};

            std::array<VkClearValue, 2> clearColors{};
            clearColors[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearColors[1].depthStencil = {1.0f, 0};

            renderPassBeginInfo.clearValueCount = clearColors.size();
            renderPassBeginInfo.pClearValues = clearColors.data();

            vkCmdBeginRenderPass(m_CommandBuffers[currentFrameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_MainGraphicsPipeline.pipeline);

            vkCmdSetViewport(m_CommandBuffers[currentFrameIndex], 0, 1, &m_RenderViewport);

            vkCmdSetScissor(m_CommandBuffers[currentFrameIndex], 0, 1, &m_RenderScissor);

            glm::mat4 viewMatrix;
            glm::mat4 projectionMatrix;

            if (m_PrimaryCamera) {
                viewMatrix = m_PrimaryCamera->GetViewMatrix();
                projectionMatrix = glm::perspective(glm::radians(m_PrimaryCamera->FOV), m_Settings.RenderWidth / (float) m_Settings.RenderHeight, m_Settings.CameraNear, CAMERA_FAR);

                // invert Y axis, glm was meant for OpenGL which inverts the Y axis.
                projectionMatrix[1][1] *= -1;

                for (RenderModel &renderModel : m_RenderModels) {
                    renderModel.matricesUBO.modelMatrix = renderModel.model->GetModelMatrix();

                    renderModel.matricesUBO.viewMatrix = viewMatrix;
                    renderModel.matricesUBO.projectionMatrix = projectionMatrix;

                    SDL_memcpy(renderModel.matricesUBOBuffer.mappedData, &renderModel.matricesUBO, sizeof(renderModel.matricesUBO));

                    // vertex buffer binding!!
                    VkDeviceSize mainOffsets[] = {0};
                    vkCmdBindVertexBuffers(m_CommandBuffers[currentFrameIndex], 0, 1, &renderModel.vertexBuffer.buffer, mainOffsets);

                    vkCmdBindIndexBuffer(m_CommandBuffers[currentFrameIndex], renderModel.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                    size_t uniformBufferSize = sizeof(MatricesUBO);

                    // update descriptor set with buffer
                    VkDescriptorBufferInfo bufferInfo{};
                    bufferInfo.buffer = renderModel.matricesUBOBuffer.buffer;
                    bufferInfo.offset = 0;
                    bufferInfo.range = uniformBufferSize;

                    // update descriptor set with image
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = renderModel.diffTextureImageView;
                    imageInfo.sampler = renderModel.diffTextureSampler;

                    std::array<VkWriteDescriptorSet, 2> descriptorWrites;
                    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[0].pNext = nullptr;
                    descriptorWrites[0].dstSet = m_RenderDescriptorSet; // Ignored
                    descriptorWrites[0].dstBinding = 0;
                    descriptorWrites[0].dstArrayElement = 0;
                    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptorWrites[0].descriptorCount = 1;
                    descriptorWrites[0].pBufferInfo = &bufferInfo;
                    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[1].pNext = nullptr;
                    descriptorWrites[1].dstSet = m_RenderDescriptorSet; // Ignored
                    descriptorWrites[1].dstBinding = 1;
                    descriptorWrites[1].dstArrayElement = 0;
                    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrites[1].descriptorCount = 1;
                    descriptorWrites[1].pImageInfo = &imageInfo;

                    vkCmdPushDescriptorSet(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_MainGraphicsPipeline.layout, 0, descriptorWrites.size(), descriptorWrites.data());

                    //vkCmdBindDescriptorSets(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_MainGraphicsPipeline.layout, 0, 1, &m_RenderDescriptorSet, 0, nullptr);
                    vkCmdDrawIndexed(m_CommandBuffers[currentFrameIndex], renderModel.indexBufferSize, 1, 0, 0, 0);
                }
            }

            // WAYPOINT SHADER!
            vkCmdNextSubpass(m_CommandBuffers[currentFrameIndex], VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIWaypointGraphicsPipeline.pipeline);

            vkCmdSetViewport(m_CommandBuffers[currentFrameIndex], 0, 1, &m_RenderViewport);

            vkCmdSetScissor(m_CommandBuffers[currentFrameIndex], 0, 1, &m_RenderScissor);

            if (m_PrimaryCamera) {
                for (RenderUIWaypoint &renderUIWaypoint : m_RenderUIWaypoints) {
                    if (!renderUIWaypoint.waypoint->GetVisible()) {
                        continue;
                    }

                    // There is no model matrix for render waypoints, We already know where it is in world-space.
                    renderUIWaypoint.matricesUBO.viewMatrix = viewMatrix;
                    renderUIWaypoint.matricesUBO.projectionMatrix = projectionMatrix;

                    SDL_memcpy(renderUIWaypoint.matricesUBOBuffer.mappedData, &renderUIWaypoint.matricesUBO, sizeof(renderUIWaypoint.matricesUBO));

                    renderUIWaypoint.waypointUBO.Position = renderUIWaypoint.waypoint->GetWorldSpacePosition();

                    SDL_memcpy(renderUIWaypoint.waypointUBOBuffer.mappedData, &renderUIWaypoint.waypointUBO, sizeof(renderUIWaypoint.waypointUBO));

                    // vertex buffer binding!!
                    VkDeviceSize waypointVertexOffsets[] = {0};
                    vkCmdBindVertexBuffers(m_CommandBuffers[currentFrameIndex], 0, 1, &(m_FullscreenQuadVertexBuffer.buffer), waypointVertexOffsets);

                    vkCmdBindDescriptorSets(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIWaypointGraphicsPipeline.layout, 0, 1, &renderUIWaypoint.descriptorSet, 0, nullptr);
                    vkCmdDraw(m_CommandBuffers[currentFrameIndex], 6, 1, 0, 0);
                }
            }

            // ARROWS SHADER!
            vkCmdNextSubpass(m_CommandBuffers[currentFrameIndex], VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIArrowsGraphicsPipeline.pipeline);

            vkCmdSetViewport(m_CommandBuffers[currentFrameIndex], 0, 1, &m_RenderViewport);

            vkCmdSetScissor(m_CommandBuffers[currentFrameIndex], 0, 1, &m_RenderScissor);

            if (m_PrimaryCamera) {
                for (RenderUIArrows &renderUIArrows : m_RenderUIArrows) {
                    if (!renderUIArrows.arrows->GetVisible()) {
                        continue;
                    }

                    // index for arrowBuffers
                    int i = 0;

                    for (RenderModel &arrowRenderModel : renderUIArrows.arrowRenderModels) {
                        MatricesUBO &matricesUBO = renderUIArrows.arrowBuffers[i].first.first;
                        UIArrowsUBO &arrowsUBO = renderUIArrows.arrowBuffers[i].first.second;

                        BufferAndMemory &matricesUBOBuffer = renderUIArrows.arrowBuffers[i].second.first;
                        BufferAndMemory &arrowsUBOBuffer = renderUIArrows.arrowBuffers[i].second.second;

                        matricesUBO.modelMatrix = arrowRenderModel.model->GetModelMatrix();
                        matricesUBO.viewMatrix = viewMatrix;
                        matricesUBO.projectionMatrix = projectionMatrix;

                        SDL_memcpy(matricesUBOBuffer.mappedData, &matricesUBO, sizeof(matricesUBO));

                        arrowsUBO.Color = arrowRenderModel.diffColor;

                        SDL_memcpy(arrowsUBOBuffer.mappedData, &arrowsUBO, sizeof(arrowsUBO));

                        // vertex buffer binding!!
                        VkDeviceSize arrowsVertexOffsets[] = {0};
                        vkCmdBindVertexBuffers(m_CommandBuffers[currentFrameIndex], 0, 1, &(arrowRenderModel.vertexBuffer.buffer), arrowsVertexOffsets);

                        vkCmdBindIndexBuffer(m_CommandBuffers[currentFrameIndex], arrowRenderModel.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                        // update descriptor set with buffer
                        VkDescriptorBufferInfo bufferInfo{};
                        bufferInfo.buffer = matricesUBOBuffer.buffer;
                        bufferInfo.offset = 0;
                        bufferInfo.range = sizeof(matricesUBO);

                        // update descriptor set with buffer
                        VkDescriptorBufferInfo bufferInfo2{};
                        bufferInfo2.buffer = arrowsUBOBuffer.buffer;
                        bufferInfo2.offset = 0;
                        bufferInfo2.range = sizeof(arrowsUBO);

                        std::array<VkWriteDescriptorSet, 2> descriptorWrites;
                        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorWrites[0].pNext = nullptr;
                        descriptorWrites[0].dstSet = m_RenderDescriptorSet; // Ignored
                        descriptorWrites[0].dstBinding = 0;
                        descriptorWrites[0].dstArrayElement = 0;
                        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        descriptorWrites[0].descriptorCount = 1;
                        descriptorWrites[0].pBufferInfo = &bufferInfo;
                        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorWrites[1].pNext = nullptr;
                        descriptorWrites[1].dstSet = m_RenderDescriptorSet; // Ignored
                        descriptorWrites[1].dstBinding = 1;
                        descriptorWrites[1].dstArrayElement = 0;
                        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                        descriptorWrites[1].descriptorCount = 1;
                        descriptorWrites[1].pBufferInfo = &bufferInfo2;

                        vkCmdPushDescriptorSet(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIArrowsGraphicsPipeline.layout, 0, descriptorWrites.size(), descriptorWrites.data());
                        vkCmdDrawIndexed(m_CommandBuffers[currentFrameIndex], arrowRenderModel.indexBufferSize, 1, 0, 0, 0);

                        i++;
                    }
                }
            }

            vkCmdEndRenderPass(m_CommandBuffers[currentFrameIndex]);
        }

        //ChangeImageLayout(m_RenderImageAndMemory.image, m_RenderImageFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        {
            // begin a render pass, this could be for example HDR pass, SSAO pass, lighting pass.
            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = m_RescaleRenderPass;
            renderPassBeginInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
            renderPassBeginInfo.renderArea.offset = {0, 0};
            renderPassBeginInfo.renderArea.extent = {m_Settings.DisplayWidth, m_Settings.DisplayHeight};

            std::array<VkClearValue, 2> clearColors{};
            clearColors[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearColors[1].depthStencil = {1.0f, 0};

            renderPassBeginInfo.clearValueCount = clearColors.size();
            renderPassBeginInfo.pClearValues = clearColors.data();

            vkCmdBeginRenderPass(m_CommandBuffers[currentFrameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_RescaleGraphicsPipeline.pipeline);

            vkCmdSetViewport(m_CommandBuffers[currentFrameIndex], 0, 1, &m_DisplayViewport);

            vkCmdSetScissor(m_CommandBuffers[currentFrameIndex], 0, 1, &m_DisplayScissor);

            // vertex buffer binding!!
            VkDeviceSize mainOffsets[] = {0};
            vkCmdBindVertexBuffers(m_CommandBuffers[currentFrameIndex], 0, 1, &(m_FullscreenQuadVertexBuffer.buffer), mainOffsets);

            vkCmdBindDescriptorSets(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_RescaleGraphicsPipeline.layout, 0, 1, &m_RescaleDescriptorSet, 0, nullptr);
            vkCmdDraw(m_CommandBuffers[currentFrameIndex], 6, 1, 0, 0);

            // Panel Shader
            vkCmdNextSubpass(m_CommandBuffers[currentFrameIndex], VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIPanelGraphicsPipeline.pipeline);

            vkCmdSetViewport(m_CommandBuffers[currentFrameIndex], 0, 1, &m_DisplayViewport);

            vkCmdSetScissor(m_CommandBuffers[currentFrameIndex], 0, 1, &m_DisplayScissor);

            for (RenderUIPanel &renderUIPanel : m_UIPanels) {
                if (!renderUIPanel.panel->GetVisible()) {
                    continue;
                }

                // vertex buffer binding!!
                VkDeviceSize panelVertexOffsets[] = {0};
                vkCmdBindVertexBuffers(m_CommandBuffers[currentFrameIndex], 0, 1, &(m_FullscreenQuadVertexBuffer.buffer), panelVertexOffsets);

                renderUIPanel.ubo.Dimensions = renderUIPanel.panel->GetDimensions();
                
                /* Double the scales for some odd reason.. */
                renderUIPanel.ubo.Dimensions.z *= 2;
                renderUIPanel.ubo.Dimensions.w *= 2;

                /* Convert [0, 1] to [-1, 1] */
                renderUIPanel.ubo.Dimensions.x *= 2;
                renderUIPanel.ubo.Dimensions.x -= 1;
                
                renderUIPanel.ubo.Dimensions.y *= 2;
                renderUIPanel.ubo.Dimensions.y -= 1;

                renderUIPanel.ubo.Depth = renderUIPanel.panel->GetDepth();

                SDL_memcpy(renderUIPanel.uboBuffer.mappedData, &(renderUIPanel.ubo), sizeof(renderUIPanel.ubo));

                // update descriptor set with image
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = renderUIPanel.textureView;
                imageInfo.sampler = renderUIPanel.textureSampler;

                // update descriptor set with UBO
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = renderUIPanel.uboBuffer.buffer;
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(renderUIPanel.ubo);

                std::array<VkWriteDescriptorSet, 2> descriptorWrites;
                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].pNext = nullptr;
                descriptorWrites[0].dstSet = m_RescaleDescriptorSet; // Ignored
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;
                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].pNext = nullptr;
                descriptorWrites[1].dstSet = m_RescaleDescriptorSet; // Ignored
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfo;

                vkCmdPushDescriptorSet(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIPanelGraphicsPipeline.layout, 0, descriptorWrites.size(), descriptorWrites.data());
                vkCmdDraw(m_CommandBuffers[currentFrameIndex], 6, 1, 0, 0);
            }

            // Label Shader
            vkCmdNextSubpass(m_CommandBuffers[currentFrameIndex], VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UILabelGraphicsPipeline.pipeline);

            vkCmdSetViewport(m_CommandBuffers[currentFrameIndex], 0, 1, &m_DisplayViewport);

            vkCmdSetScissor(m_CommandBuffers[currentFrameIndex], 0, 1, &m_DisplayScissor);

            for (RenderUILabel &renderUILabel : m_UILabels) {
                if (!renderUILabel.label->GetVisible()) {
                    continue;
                }

                // vertex buffer binding!!
                VkDeviceSize labelVertexOffsets[] = {0};

                renderUILabel.ubo.PositionOffset = renderUILabel.label->GetPosition();
                renderUILabel.ubo.PositionOffset.x *= 2;
                renderUILabel.ubo.PositionOffset.y *= 2;

                renderUILabel.ubo.Depth = renderUILabel.label->GetDepth();

                SDL_memcpy(renderUILabel.uboBuffer.mappedData, &(renderUILabel.ubo), sizeof(renderUILabel.ubo));

                size_t i = 0;
                for (auto &shaderData : renderUILabel.textureShaderData) {
                    auto &glyph = renderUILabel.label->Glyphs[i++];

                    vkCmdBindVertexBuffers(m_CommandBuffers[currentFrameIndex], 0, 1, &(glyph.glyphBuffer.value().second.buffer), labelVertexOffsets);

                    glyph.glyphUBO.Offset = glyph.offset;
                    
                    SDL_memcpy(glyph.glyphUBOBuffer.mappedData, &(glyph.glyphUBO), sizeof(glyph.glyphUBO));

                    // update descriptor set with image
                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = shaderData.second.first;
                    imageInfo.sampler = shaderData.second.second;

                    // update descriptor set with UBO
                    VkDescriptorBufferInfo labelBufferInfo{};
                    labelBufferInfo.offset = 0;
                    labelBufferInfo.range = sizeof(renderUILabel.ubo);
                    labelBufferInfo.buffer = renderUILabel.uboBuffer.buffer;

                    // update descriptor set with UBO
                    VkDescriptorBufferInfo glyphBufferInfo{};
                    glyphBufferInfo.offset = 0;
                    glyphBufferInfo.range = sizeof(glyph.glyphUBO);
                    glyphBufferInfo.buffer = glyph.glyphUBOBuffer.buffer;

                    std::array<VkWriteDescriptorSet, 3> descriptorWrites;
                    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[0].pNext = nullptr;
                    descriptorWrites[0].dstSet = m_RenderDescriptorSet; // Ignored
                    descriptorWrites[0].dstBinding = 0;
                    descriptorWrites[0].dstArrayElement = 0;
                    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptorWrites[0].descriptorCount = 1;
                    descriptorWrites[0].pBufferInfo = &labelBufferInfo;
                    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[1].pNext = nullptr;
                    descriptorWrites[1].dstSet = m_RenderDescriptorSet; // Ignored
                    descriptorWrites[1].dstBinding = 1;
                    descriptorWrites[1].dstArrayElement = 0;
                    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrites[1].descriptorCount = 1;
                    descriptorWrites[1].pImageInfo = &imageInfo;
                    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[2].pNext = nullptr;
                    descriptorWrites[2].dstSet = m_RenderDescriptorSet; // Ignored
                    descriptorWrites[2].dstBinding = 2;
                    descriptorWrites[2].dstArrayElement = 0;
                    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptorWrites[2].descriptorCount = 1;
                    descriptorWrites[2].pBufferInfo = &glyphBufferInfo;

                    vkCmdPushDescriptorSet(m_CommandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_UILabelGraphicsPipeline.layout, 0, descriptorWrites.size(), descriptorWrites.data());
                    vkCmdDraw(m_CommandBuffers[currentFrameIndex], 6, 1, 0, 0);
                }
            }

            vkCmdEndRenderPass(m_CommandBuffers[currentFrameIndex]);
        }

        //ChangeImageLayout(m_RenderImageAndMemory.image, m_RenderImageFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

#ifdef LOG_FRAME
        afterRenderTime = high_resolution_clock::now();

        fmt::println("Time spent rendering: {:.5f}ms", (duration_cast<duration<double, std::milli>>(afterRenderTime - afterUpdateTime).count()));
#endif

        if (vkEndCommandBuffer(m_CommandBuffers[currentFrameIndex]) != VK_SUCCESS)
            throw std::runtime_error(engineError::COMMAND_BUFFER_END_FAILURE);

        // we recorded all the commands, submit them.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[currentFrameIndex];
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[currentFrameIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[currentFrameIndex];

        VkResult queueSubmitResult = vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[currentFrameIndex]);
        if (queueSubmitResult != VK_SUCCESS)
            throw std::runtime_error(fmt::format(engineError::QUEUE_SUBMIT_FAILURE, string_VkResult(queueSubmitResult)));

        // we finished, now we should present the frame.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[currentFrameIndex];

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;

        presentInfo.pImageIndices = &imageIndex;

        presentInfo.pResults = nullptr;

        vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

        currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

#ifdef LOG_FRAME
        postRenderTime = high_resolution_clock::now();

        fmt::println("Time spent post render: {:.5f}ms", (duration_cast<duration<double, std::milli>>(postRenderTime - afterRenderTime).count()));
#endif
    }

    //fixedUpdateThread.join();
}

Engine::~Engine() {
    DisconnectFromServer();

    StopHostingGameServer();

    GameNetworkingSockets_Kill();
}

void Engine::InitRenderer(Settings &settings, const Camera *primaryCamera) {
    m_Settings = &settings;

    m_Renderer = new Renderer(settings, primaryCamera);

    m_Renderer->Init();

    m_Renderer->RegisterSDLEventListener(std::bind(&Engine::CheckButtonClicks, this, std::placeholders::_1), SDL_EVENT_MOUSE_BUTTON_UP);
}

void Engine::InitNetworking() {
    SteamDatagramErrMsg errMsg;

    if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
        throw std::runtime_error(fmt::format("Failed to initialize networking! {}", errMsg));
    }

    m_NetworkingSockets = SteamNetworkingSockets();
}

void Engine::RegisterUIButtonListener(const std::function<void(std::string)> listener) {
    m_UIButtonListeners.push_back(listener);
}

Renderer *Engine::GetRenderer() {
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

void Engine::StartRenderer() {
    m_Renderer->Start();
}

void Engine::LoadUIFile(const std::string &name) {
    if (m_Renderer == nullptr) {
        return;
    }

    EngineSharedContext sharedContext = m_Renderer->GetSharedContext();
    std::vector<UI::GenericElement *> UIElements = UI::LoadUIFile(sharedContext, name);
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

    using namespace rapidxml;

    std::ifstream sceneFile(path.data(), std::ios::binary | std::ios::ate);

    if (!sceneFile.good())
        return false;

    std::vector<char> sceneRawXML(static_cast<int>(sceneFile.tellg()) + 1);
    
    sceneFile.seekg(0);
    sceneFile.read(sceneRawXML.data(), sceneRawXML.size());

    xml_document<char> sceneXML;

    sceneXML.parse<0>(sceneRawXML.data());

    xml_node<char> *sceneNode = sceneXML.first_node("Scene");

    for (xml_node <char> *objectNode = sceneNode->first_node("Object"); objectNode; objectNode = objectNode->next_sibling("Object")) {
        
        xml_node<char> *positionNode = objectNode->first_node("Position");
        UTILASSERT(positionNode);
        std::string_view positionStr(positionNode->value());
        std::vector<std::string> positionData = split(positionStr, ' ');
        
        glm::vec3 position = glm::vec3(std::stof(positionData[0]), std::stof(positionData[1]), std::stof(positionData[2]));

        xml_node<char> *rotationNode = objectNode->first_node("Rotation");
        UTILASSERT(rotationNode);
        std::string_view rotationStr(rotationNode->value());
        std::vector<std::string> rotationData = split(rotationStr, ' ');
        glm::vec3 rotation = glm::vec3(std::stof(rotationData[0]), std::stof(rotationData[1]), std::stof(rotationData[2]));

        xml_node<char> *scaleNode = objectNode->first_node("Scale");
        UTILASSERT(scaleNode);
        std::string_view scaleStr(scaleNode->value());
        std::vector<std::string> scaleData = split(scaleStr, ' ');
        glm::vec3 scale = glm::vec3(std::stof(scaleData[0]), std::stof(scaleData[1]), std::stof(scaleData[2]));

        Object *object = new Object(position, rotation, scale);

        xml_node<char> *objectIDNode = objectNode->first_node("ObjectID");
        UTILASSERT(objectIDNode);
        std::string objectIDStr(rotationNode->value());
        
        object->SetObjectID(std::stoi(objectIDStr));

        m_Objects.push_back(object);

        for (xml_node<char> *modelNode = objectNode->first_node("Model"); modelNode; modelNode = modelNode->next_sibling("Model")) {
            Model *model = new Model();

            xml_node<char> *positionNode = modelNode->first_node("Position");
            UTILASSERT(positionNode);
            std::string_view positionStr(positionNode->value());
            std::vector<std::string> positionData = split(positionStr, ' ');
            model->SetPosition(glm::vec3(std::stof(positionData[0]), std::stof(positionData[1]), std::stof(positionData[2])));

            xml_node<char> *rotationNode = modelNode->first_node("Rotation");
            UTILASSERT(rotationNode);
            std::string_view rotationStr(rotationNode->value());
            std::vector<std::string> rotationData = split(rotationStr, ' ');
            model->SetRotation(glm::vec3(std::stof(rotationData[0]), std::stof(rotationData[1]), std::stof(rotationData[2])));

            xml_node<char> *scaleNode = modelNode->first_node("Scale");
            UTILASSERT(scaleNode);
            std::string_view scaleStr(scaleNode->value());
            std::vector<std::string> scaleData = split(scaleStr, ' ');
            model->SetScale(glm::vec3(std::stof(scaleData[0]), std::stof(scaleData[1]), std::stof(scaleData[2])));

            for (xml_node<char> *meshNode = modelNode->first_node("Mesh"); meshNode; meshNode = meshNode->next_sibling("Mesh")) {
                Mesh mesh;

                xml_node<char> *diffuseNode = meshNode->first_node("Diffuse");
                UTILASSERT(diffuseNode);
                std::string_view diffuseStr(diffuseNode->value());
                std::vector<std::string> diffuseData = split(diffuseStr, ' ');
                mesh.diffuse = glm::vec3(std::stof(diffuseData[0]), std::stof(diffuseData[1]), std::stof(diffuseData[2]));

                xml_node<char> *indicesNode = meshNode->first_node("Indices");
                UTILASSERT(indicesNode);
                std::string_view indicesStr(indicesNode->value());
                std::vector<std::string> indicesData = split(indicesStr, ',');
                mesh.indices.resize(indicesData.size());

                size_t i = 0;
                for (const std::string &str : indicesData) {
                    mesh.indices[i] = std::stoi(str);
                    i++;
                }

                xml_node<char> *diffuseMapPathNode = meshNode->first_node("DiffuseMap");
                UTILASSERT(diffuseMapPathNode);
                std::string_view diffuseMapPathStr(diffuseMapPathNode->value());
                mesh.diffuseMapPath = diffuseMapPathStr;

                for (xml_node<char> *vertexNode = meshNode->first_node("Vertex"); vertexNode; vertexNode = vertexNode->next_sibling("Vertex")) {
                    Vertex vertex;

                    xml_node<char> *vertexPositionNode = vertexNode->first_node("Position");
                    UTILASSERT(vertexPositionNode);
                    std::string_view vertexPositionStr(vertexPositionNode->value());
                    std::vector<std::string> vertexPositionData = split(vertexPositionStr, ' ');
                    vertex.Position = glm::vec3(std::stof(vertexPositionData[0]), std::stof(vertexPositionData[1]), std::stof(vertexPositionData[2]));

                    xml_node<char> *vertexNormalNode = vertexNode->first_node("Normal");
                    UTILASSERT(vertexNormalNode);
                    std::string_view vertexNormalStr(vertexNormalNode->value());
                    std::vector<std::string> vertexNormalData = split(vertexNormalStr, ' ');
                    vertex.Normal = glm::vec3(std::stof(vertexNormalData[0]), std::stof(vertexNormalData[1]), std::stof(vertexNormalData[2]));

                    xml_node<char> *vertexTexCoordNode = vertexNode->first_node("TexCoord");
                    UTILASSERT(vertexTexCoordNode);
                    std::string_view vertexTexCoordStr(vertexTexCoordNode->value());
                    std::vector<std::string> vertexTexCoordData = split(vertexTexCoordStr, ' ');
                    vertex.TexCoord = glm::vec2(std::stof(vertexTexCoordData[0]), std::stof(vertexTexCoordData[1]));

                    glm::vec3 boundingBoxMin;
                    glm::vec3 boundingBoxMax;

                    std::array<glm::vec3, 2> modelBoundingBox = model->GetRawBoundingBox();

                    boundingBoxMin.x = std::max(modelBoundingBox[0].x, vertex.Position.x);
                    boundingBoxMin.y = std::max(modelBoundingBox[0].y, vertex.Position.y);
                    boundingBoxMin.z = std::max(modelBoundingBox[0].z, vertex.Position.z);
                    boundingBoxMax.x = std::min(modelBoundingBox[1].x, vertex.Position.x);
                    boundingBoxMax.y = std::min(modelBoundingBox[1].y, vertex.Position.y);
                    boundingBoxMax.z = std::min(modelBoundingBox[1].z, vertex.Position.z);

                    model->SetBoundingBox({boundingBoxMin, boundingBoxMax});

                    mesh.vertices.push_back(vertex);
                }

                model->meshes.push_back(mesh);
            }

            object->AddModelAttachment(model);

            if (m_Renderer) {
                m_Renderer->LoadModel(model);
            }
        }
    }

    return true;
}

void Engine::ExportScene(const std::string &path) {
    using namespace rapidxml;

    xml_document<char> sceneXML;

    xml_node<char> *node = sceneXML.allocate_node(node_type::node_element, "Scene");
    sceneXML.append_node(node);

    for (Object *object : m_Objects) {
        xml_node<char> *objectNode = sceneXML.allocate_node(node_type::node_element, "Object");
        node->append_node(objectNode);

        xml_node<char> *objectIDNode = sceneXML.allocate_node(node_type::node_element, "ObjectID", fmt::to_string(object->GetObjectID()).c_str());
        objectNode->append_node(objectIDNode);
        
        glm::vec3 position = object->GetPosition();
        glm::vec3 rotation = object->GetRotation();
        glm::vec3 scale = object->GetScale();

        std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
        xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
        objectNode->append_node(positionNode);

        std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
        xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
        objectNode->append_node(rotationNode);

        std::string scaleStr = fmt::format("{} {} {}", scale.x, scale.y, scale.z);
        xml_node<char> *scaleNode = sceneXML.allocate_node(node_type::node_element, "Scale", sceneXML.allocate_string(scaleStr.c_str()));
        objectNode->append_node(scaleNode);

        for (Model *model : object->GetModelAttachments()) {
            xml_node<char> *modelNode = sceneXML.allocate_node(node_type::node_element, "Model");
            objectNode->append_node(modelNode);

            glm::vec3 position = model->GetPosition();
            glm::vec3 rotation = model->GetRotation();
            glm::vec3 scale = model->GetScale();

            std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
            xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
            modelNode->append_node(positionNode);

            std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
            xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
            modelNode->append_node(rotationNode);

            std::string scaleStr = fmt::format("{} {} {}", scale.x, scale.y, scale.z);
            xml_node<char> *scaleNode = sceneXML.allocate_node(node_type::node_element, "Scale", sceneXML.allocate_string(scaleStr.c_str()));
            modelNode->append_node(scaleNode);

            for (Mesh &mesh : model->meshes) {
                xml_node<char> *meshNode = sceneXML.allocate_node(node_type::node_element, "Mesh");
                modelNode->append_node(meshNode);

                std::string diffuseStr = fmt::format("{} {} {}", mesh.diffuse.x, mesh.diffuse.y, mesh.diffuse.z);
                xml_node<char> *diffuseNode = sceneXML.allocate_node(node_type::node_element, "Diffuse", sceneXML.allocate_string(diffuseStr.c_str()));
                meshNode->append_node(diffuseNode);

                std::string indicesStr = fmt::to_string(fmt::join(mesh.indices, ","));
                xml_node<char> *indicesNode = sceneXML.allocate_node(node_type::node_element, "Indices", sceneXML.allocate_string(indicesStr.c_str()));
                meshNode->append_node(indicesNode);

                xml_node<char> *diffuseMapPathNode = sceneXML.allocate_node(node_type::node_element, "DiffuseMap", mesh.diffuseMapPath.c_str());
                meshNode->append_node(diffuseMapPathNode);

                for (Vertex vert : mesh.vertices) {
                    xml_node<char> *vertexNode = sceneXML.allocate_node(node_type::node_element, "Vertex");
                    meshNode->append_node(vertexNode);

                    std::string vertPositionStr = fmt::format("{} {} {}", vert.Position.x, vert.Position.y, vert.Position.z);
                    xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(vertPositionStr.c_str()));
                    vertexNode->append_node(positionNode);
                    
                    std::string vertNormalStr = fmt::format("{} {} {}", vert.Normal.x, vert.Normal.y, vert.Normal.z);
                    xml_node<char> *normalNode = sceneXML.allocate_node(node_type::node_element, "Normal", sceneXML.allocate_string(vertNormalStr.c_str()));
                    vertexNode->append_node(normalNode);
                    
                    std::string vertTexCoordStr = fmt::format("{} {}", vert.TexCoord.x, vert.TexCoord.y);
                    xml_node<char> *texCoordNode = sceneXML.allocate_node(node_type::node_element, "TexCoord", sceneXML.allocate_string(vertTexCoordStr.c_str()));
                    vertexNode->append_node(texCoordNode);
                }
            }
        }
    }

    std::ofstream targetFile(path);
    targetFile << sceneXML;

    sceneXML.clear();
}

void Engine::ConnectToGameServer(SteamNetworkingIPAddr ipAddr) {
    SteamNetworkingConfigValue_t opt{};    

    opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void *)onConnectionStatusChangedCallback);

    HSteamNetConnection netConnection = m_NetworkingSockets->ConnectByIPAddress(ipAddr, 1, &opt);
    
    if (netConnection == k_HSteamNetConnection_Invalid) {
        throw std::runtime_error("Failed to connect to server!");
    }

    m_NetConnections.push_back(netConnection);

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
                auto conn = std::find(m_NetConnections.begin(), m_NetConnections.end(), callbackInfo->m_hConn);
                UTILASSERT(conn != m_NetConnections.end());

                m_NetConnections.erase(conn);

                m_NetworkingSockets->CloseConnection(callbackInfo->m_hConn, 0, nullptr, false);
            }

            break;
        case k_ESteamNetworkingConnectionState_Connecting:
            if (m_NetworkingThreadStatus & NETWORKING_THREAD_ACTIVE_SERVER) {
                fmt::println("We're getting a connection!");

                /* This callback only happens when a new client is connecting. */
                UTILASSERT(std::find(m_NetConnections.begin(), m_NetConnections.end(), callbackInfo->m_hConn) == m_NetConnections.end());

                if (m_NetworkingSockets->AcceptConnection(callbackInfo->m_hConn) != k_EResultOK) {
                    m_NetworkingSockets->CloseConnection(callbackInfo->m_hConn, 0, nullptr, false);

                    break;
                }
                
                if (!m_NetworkingSockets->SetConnectionPollGroup(callbackInfo->m_hConn, m_NetPollGroup)) {
                    m_NetworkingSockets->CloseConnection(callbackInfo->m_hConn, 0, nullptr, false);

                    break;
                }

                SendFullUpdateToConnection(callbackInfo->m_hConn);

                m_NetConnections.push_back(callbackInfo->m_hConn);
            }

            break;
        
        case k_ESteamNetworkingConnectionState_Connected:
            break;

        default:
            break;
    }
}

UI::GenericElement *Engine::GetElementByID(const std::string &id) {
    for (UI::GenericElement *&element : m_UIElements) {
        if (element->id == id) {
            return element;
        }
    }

    return nullptr;
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
    if (m_NetworkingThreadStatus != NETWORKING_THREAD_INACTIVE) {
        return;
    }

    fmt::println("Initializing network thread to {}!", (int)status);

    if (status == NETWORKING_THREAD_ACTIVE_CLIENT) {
        m_NetworkingThread = std::thread(&Engine::NetworkingThreadClient_Main, this);
    } else if (status == NETWORKING_THREAD_ACTIVE_SERVER) {
        m_NetworkingThread = std::thread(&Engine::NetworkingThreadServer_Main, this);
    }
}

void Engine::NetworkingThreadClient_Main() {
    if (m_NetConnections.empty()) {
        throw std::runtime_error("Networking Thread initialized with no networking connection!");
    }

    fmt::println("Started client networking thread!");
    m_NetworkingThreadStatus |= NETWORKING_THREAD_ACTIVE_CLIENT;

    using namespace std::chrono;

    high_resolution_clock::time_point lastTickTime = high_resolution_clock::now();
    double accumulativeTickTime = 0;

    while (!m_NetworkingThreadShouldQuit) {
        high_resolution_clock::time_point now = high_resolution_clock::now();

        double tickDeltaTime = (duration_cast<duration<double>>(now - lastTickTime).count());

        accumulativeTickTime += tickDeltaTime;

        /* hardcoded 64 tick, to be replaced. */
        if (accumulativeTickTime < (1.0f/64.0f)) {
            std::this_thread::sleep_for(milliseconds(8));  /* keep waking up at half the time just incase */
            continue;
        }

        accumulativeTickTime -= (1.0f / 64.0f);

        m_CallbackInstance = this;
        m_NetworkingSockets->RunCallbacks();

        /* Receiving */
        for (HSteamNetConnection netConnection : m_NetConnections) {
            ISteamNetworkingMessage *incomingMessages;
            int msgCount = m_NetworkingSockets->ReceiveMessagesOnConnection(netConnection, &incomingMessages, 1);

            if (msgCount < 0) {
                throw std::runtime_error("Error receiving messages from server!");
            }
            if (msgCount > 0) {
                for (int i = 0; i < msgCount; i++) {
                    ISteamNetworkingMessage *incomingMessage = incomingMessages + (i * sizeof(ISteamNetworkingMessage));

                    if (incomingMessage->GetSize() <= sizeof(int) + sizeof(size_t)) {
                        fmt::println("Invalid packet!");
                    } else {
                        const void *data = incomingMessage->GetData();
                        
                        std::vector<std::byte> message{reinterpret_cast<const std::byte *>(data), reinterpret_cast<const std::byte *>(incomingMessage->GetSize())};

                        Networking_StatePacket packet = DeserializePacket(message);

                        fmt::println("New state packet just dropped! {} objects sent by server", packet.objects.size());
                    }
                    
                    incomingMessage->Release();
                }
            }
        }


        /* Sending */
        // ...
    }

    fmt::println("Stopping client networking thread!");

    m_NetworkingThreadStatus &= ~NETWORKING_THREAD_ACTIVE_CLIENT;
}



void Engine::NetworkingThreadServer_Main() {
    if (!m_NetListenSocket) {
        throw std::runtime_error("Networking Thread initialized with no networking connection!");
    }

    fmt::println("Started server networking thread!");
    m_NetworkingThreadStatus |= NETWORKING_THREAD_ACTIVE_SERVER;

    using namespace std::chrono;

    high_resolution_clock::time_point lastTickTime = high_resolution_clock::now();
    double accumulativeTickTime = 0;

    while (!m_NetworkingThreadShouldQuit) {
        high_resolution_clock::time_point now = high_resolution_clock::now();

        double tickDeltaTime = (duration_cast<duration<double>>(now - lastTickTime).count());

        accumulativeTickTime += tickDeltaTime;

        /* hardcoded 64 tick, to be replaced. */
        if (accumulativeTickTime < (1.0f/64.0f)) {
            std::this_thread::sleep_for(milliseconds(8));  /* keep waking up at half the time just incase */
            continue;
        }

        accumulativeTickTime -= (1.0f / 64.0f);
        
        m_CallbackInstance = this;
        m_NetworkingSockets->RunCallbacks();

        ISteamNetworkingMessage *incomingMessages;
        int msgCount = m_NetworkingSockets->ReceiveMessagesOnPollGroup(m_NetPollGroup, &incomingMessages, 1);

        if (msgCount < 0) {
            throw std::runtime_error("Error receiving messages from a client!");
        }
        if (msgCount > 0) {
            for (int i = 0; i < msgCount; i++) {
                ISteamNetworkingMessage *incomingMessage = incomingMessages + (i * sizeof(ISteamNetworkingMessage));

                if (incomingMessage->GetSize() <= sizeof(int) + sizeof(size_t)) {
                    fmt::println("Invalid packet!");
                } else {
                    fmt::println("Got a packet.. what do I do with that?");
                }
                
                incomingMessage->Release();
            }
        }

        lastTickTime = now;
    }

    fmt::println("Stopping server networking thread!");

    m_NetworkingThreadStatus &= ~NETWORKING_THREAD_ACTIVE_SERVER;
}

/* This should be rewritten, I'll get to it after I get a proper working demo. */
void Engine::DisconnectFromServer() {
    if (m_NetworkingThreadStatus & NETWORKING_THREAD_ACTIVE_CLIENT) {
        m_NetworkingThreadShouldQuit = true;

        m_NetworkingThread.join();

        for (HSteamNetConnection netConnection : m_NetConnections) {
            m_NetworkingSockets->CloseConnection(netConnection, 0, nullptr, true);
        }
    }
}

void Engine::DisconnectClientFromServer(HSteamNetConnection connection) {
    auto it = std::find(m_NetConnections.begin(), m_NetConnections.end(), connection);
    UTILASSERT(it != m_NetConnections.end());

    m_NetworkingSockets->CloseConnection(connection, 0, nullptr, true);

    m_NetConnections.erase(it);
}

void Engine::StopHostingGameServer() {
    if (m_NetworkingThreadStatus & NETWORKING_THREAD_ACTIVE_SERVER) {
        m_NetworkingThreadShouldQuit = true;

        m_NetworkingThread.join();

        while (m_NetConnections.size() != 0) {
            DisconnectClientFromServer(m_NetConnections[0]);
        }

        if (m_NetListenSocket != k_HSteamListenSocket_Invalid) {
            m_NetworkingSockets->CloseListenSocket(m_NetListenSocket);
        }
    }
}

Networking_StatePacket Engine::DeserializePacket(std::vector<std::byte> &serializedPacket) {
    UTILASSERT(serializedPacket.size() >= sizeof(size_t));

    Networking_StatePacket statePacket{};

    size_t objectsCount;
    Deserialize(serializedPacket, objectsCount);

    for (size_t i = 0; i < objectsCount; i++) {
        Networking_Object objectPacket;

        DeserializeNetworkingObject({serializedPacket.begin() + sizeof(size_t), serializedPacket.end()}, objectPacket);
    }

    return statePacket;
}

void Engine::DeserializeNetworkingObject(std::vector<std::byte> serializedObjectPacket, Networking_Object &dest) {
    /* objectID + position/rotation/scale + modelAttachments size */
    UTILASSERT(serializedObjectPacket.size() >= sizeof(int) + (sizeof(float) * 9) + sizeof(size_t));

    Deserialize(serializedObjectPacket, dest.ObjectID);

    Deserialize(serializedObjectPacket, dest.position.x);
    Deserialize(serializedObjectPacket, dest.position.y);
    Deserialize(serializedObjectPacket, dest.position.z);

    Deserialize(serializedObjectPacket, dest.rotation.x);
    Deserialize(serializedObjectPacket, dest.rotation.y);
    Deserialize(serializedObjectPacket, dest.rotation.z);

    Deserialize(serializedObjectPacket, dest.scale.x);
    Deserialize(serializedObjectPacket, dest.scale.y);
    Deserialize(serializedObjectPacket, dest.scale.z);

    size_t modelAttachmentsSize;
    Deserialize(serializedObjectPacket, modelAttachmentsSize);

    for (size_t i = 0; i < modelAttachmentsSize; i++) {
        Networking_Model modelPacket{};

        DeserializeNetworkingModel(serializedObjectPacket, modelPacket);

        dest.modelAttachments.push_back(modelPacket);
    }
}



void Engine::DeserializeNetworkingModel(std::vector<std::byte> serializedModelPacket, Networking_Model &dest) {
    UTILASSERT(serializedModelPacket.size() >= sizeof(int) + (sizeof(float) * 9) + sizeof(size_t));

    Deserialize(serializedModelPacket, dest.modelID);

    Deserialize(serializedModelPacket, dest.position.x);
    Deserialize(serializedModelPacket, dest.position.y);
    Deserialize(serializedModelPacket, dest.position.z);

    Deserialize(serializedModelPacket, dest.rotation.x);
    Deserialize(serializedModelPacket, dest.rotation.y);
    Deserialize(serializedModelPacket, dest.rotation.z);

    Deserialize(serializedModelPacket, dest.scale.x);
    Deserialize(serializedModelPacket, dest.scale.y);
    Deserialize(serializedModelPacket, dest.scale.z);

    Deserialize(serializedModelPacket, dest.modelName);
}

void Engine::SendFullUpdateToConnection(HSteamNetConnection connection) {
    Networking_StatePacket statePacket{};
    
    for (Object *object : m_Objects) {
        Networking_Object objectPacket{};

        objectPacket.ObjectID = object->GetObjectID();
        
        objectPacket.position = object->GetPosition();
        objectPacket.rotation = object->GetRotation();
        objectPacket.scale = object->GetScale();

        for (Model *model : object->GetModelAttachments()) {
            Networking_Model modelPacket{};

            modelPacket.modelID = model->GetModelID();

            modelPacket.position = model->GetRawPosition();
            modelPacket.rotation = model->GetRawRotation();
            modelPacket.scale = model->GetRawScale();

            modelPacket.modelName = model->GetOriginalPath();

            objectPacket.modelAttachments.push_back(modelPacket);
        }

        statePacket.objects.push_back(objectPacket);
    }

    std::vector<std::byte> serializedPacket;

    /* packet object size is the first element, so might aswell initialize the array as such */
    Serialize(statePacket.objects.size(), serializedPacket);
    
    for (Networking_Object &objectPacket : statePacket.objects) {
        SerializeNetworkingObject(objectPacket, serializedPacket);
    }

    m_NetworkingSockets->SendMessageToConnection(connection, serializedPacket.data(), serializedPacket.size(), k_nSteamNetworkingSend_Reliable, nullptr);
}

void Engine::SerializeNetworkingObject(Networking_Object &objectPacket, std::vector<std::byte> &dest) {
    Serialize(objectPacket.ObjectID, dest);

    Serialize(objectPacket.position.x, dest);
    Serialize(objectPacket.position.y, dest);
    Serialize(objectPacket.position.z, dest);

    Serialize(objectPacket.rotation.x, dest);
    Serialize(objectPacket.rotation.y, dest);
    Serialize(objectPacket.rotation.z, dest);

    Serialize(objectPacket.scale.x, dest);
    Serialize(objectPacket.scale.y, dest);
    Serialize(objectPacket.scale.z, dest);

    for (Networking_Model &modelPacket : objectPacket.modelAttachments) {
        SerializeNetworkingModel(modelPacket, dest);
    }
}



void Engine::SerializeNetworkingModel(Networking_Model &modelPacket, std::vector<std::byte> &dest) {
    Serialize(modelPacket.modelID, dest);

    Serialize(modelPacket.position.x, dest);
    Serialize(modelPacket.position.y, dest);
    Serialize(modelPacket.position.z, dest);

    Serialize(modelPacket.rotation.x, dest);
    Serialize(modelPacket.rotation.y, dest);
    Serialize(modelPacket.rotation.z, dest);

    Serialize(modelPacket.scale.x, dest);
    Serialize(modelPacket.scale.y, dest);
    Serialize(modelPacket.scale.z, dest);

    Serialize(modelPacket.modelName, dest);
}

template<typename T>
void Engine::Deserialize(std::vector<std::byte> &object, T &dest) {
    if constexpr (std::is_same<T, std::string>::value) {
        UTILASSERT(object.size() >= sizeof(size_t));    /* minimum size */

        size_t stringSize = *reinterpret_cast<size_t *>(object.data());
        object.erase(object.begin(), object.begin() + sizeof(size_t));

        UTILASSERT(object.size() >= stringSize);    /* Each char is 1 byte, this is valid. */

        char *string = reinterpret_cast<char *>(object.data());

        dest = std::string(string, stringSize);

        object.erase(object.begin(), object.begin() + stringSize);
    } else {
        UTILASSERT(object.size() >= sizeof(T));
        
        dest = *reinterpret_cast<T *>(object.data());

        object.erase(object.begin(), object.begin() + sizeof(T));
    }
}

template<typename T>
void Engine::Serialize(T object, std::vector<std::byte> &dest) {
    if constexpr (std::is_same<T, std::string>::value) {
        Serialize(object.size(), dest);

        for (char &c : object) {
            Serialize(c, dest);
        }
    } else {
        for (size_t i = 0; i < sizeof(T); i++) {
            std::byte *byte = reinterpret_cast<std::byte *>(&object) + i;
            dest.push_back(*byte);
        }
    }
}

Engine *Engine::m_CallbackInstance = nullptr;
