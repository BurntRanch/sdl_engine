#include "renderer/vulkanRenderer.hpp"
#include "Node/Node3D/Model3D/Model3D.hpp"
#include "common.hpp"
#include "error.hpp"
#include "renderer/GraphicsPipeline.hpp"
#include "renderer/DescriptorLayout.hpp"
#include "renderer/RenderPass.hpp"
#include "renderer/baseRenderer.hpp"
#include "util.hpp"
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>
#include <any>
#include <future>
#include <set>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

/*static std::vector<std::byte> readFile(const std::string &name) {
    std::ifstream file(name, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to read " + name + "!");
    }

    size_t fileSize = file.tellg();
    std::vector<std::byte> output(file.tellg());

    file.seekg(0);
    file.read(reinterpret_cast<char *>(output.data()), fileSize);

    return output;
}*/

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

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

/* CLASS IMPLEMENTATIONS AHEAD 
Get ready for loss of braincells!
*/

VulkanRenderer::~VulkanRenderer() {
    fmt::println("Destroying Engine!");

    if (m_EngineDevice)
        vkDeviceWaitIdle(m_EngineDevice);

    if (m_FullscreenQuadVertexBuffer.buffer && m_FullscreenQuadVertexBuffer.memory) {
        vkDestroyBuffer(m_EngineDevice, m_FullscreenQuadVertexBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, m_FullscreenQuadVertexBuffer.memory, NULL);
    }

    for (RenderMesh &renderModel : m_RenderModels)
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

    for (GraphicsPipeline *pipeline : m_Pipelines) {
        vkDestroyPipeline(m_EngineDevice, std::any_cast<VkPipeline>(pipeline->GetRawPipeline()), NULL);
        vkDestroyPipelineLayout(m_EngineDevice, std::any_cast<VkPipelineLayout>(pipeline->GetRawPipelineLayout()), NULL);
    }
    for (RenderPass *renderPass : m_RenderPasses)
        vkDestroyRenderPass(m_EngineDevice, std::any_cast<VkRenderPass>(renderPass->GetRawRenderPass()), NULL);
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

    // if (m_RenderDescriptorSetLayout)
    //     vkDestroyDescriptorSetLayout(m_EngineDevice, m_RenderDescriptorSetLayout, NULL);

    // if (m_UILabelDescriptorSetLayout)
    //     vkDestroyDescriptorSetLayout(m_EngineDevice, m_UILabelDescriptorSetLayout, NULL);

    // if (m_RescaleDescriptorSetLayout)
    //     vkDestroyDescriptorSetLayout(m_EngineDevice, m_RescaleDescriptorSetLayout, NULL);

    // if (m_UIPanelDescriptorSetLayout)
    //     vkDestroyDescriptorSetLayout(m_EngineDevice, m_UIPanelDescriptorSetLayout, NULL);

    for (VkDescriptorSetLayout &layout : m_AllocatedDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(m_EngineDevice, layout, NULL);
    }

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

std::any VulkanRenderer::CreateShaderModule(const std::vector<std::byte> &code) {
    VkShaderModuleCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = code.size();
    shaderCreateInfo.pCode = reinterpret_cast<const Uint32*>(code.data());

    VkShaderModule out = nullptr;
    if (vkCreateShaderModule(m_EngineDevice, &shaderCreateInfo, NULL, &out) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module!");

    return out;
}

void VulkanRenderer::DestroyShaderModule(std::any shaderModule) {
    vkDestroyShaderModule(m_EngineDevice, std::any_cast<VkShaderModule>(shaderModule), NULL);
}

std::any VulkanRenderer::CreateDescriptorLayout(std::vector<PipelineBinding> &pipelineBindings) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (PipelineBinding &binding : pipelineBindings) {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.binding = binding.bindingIndex;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.descriptorType = binding.type;
        descriptorSetLayoutBinding.stageFlags = binding.shaderStageBits;
        descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        bindings.push_back(descriptorSetLayoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
    descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
    descriptorSetLayoutCreateInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;

    if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &layout) != VK_SUCCESS)
        throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);

    return layout;
}

SwapChainSupportDetails VulkanRenderer::QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
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

void VulkanRenderer::CopyHostBufferToDeviceBuffer(VkBuffer hostBuffer, VkBuffer deviceBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy bufferCopy{};
    bufferCopy.srcOffset = 0;   // start
    bufferCopy.dstOffset = 0;   // also start
    bufferCopy.size = size;

    vkCmdCopyBuffer(commandBuffer, hostBuffer, deviceBuffer, 1, &bufferCopy);

    EndSingleTimeCommands(commandBuffer);
}

// first element = diffuse
// std::array<TextureImageAndMemory, 1> VulkanRenderer::LoadTexturesFromMesh(const Mesh3D &mesh, bool recordAllocations) {
//     std::array<TextureImageAndMemory, 1> textures;
    
//     if (!mesh.diffuseMapPath.empty()) {
//         std::filesystem::path path;

//         if (!mesh.diffuseMapPath.has_root_path()) {
//             // https://stackoverflow.com/a/73927710
//             auto rel = std::filesystem::relative(mesh.diffuseMapPath, "resources");
//             // map_Kd resources/brown_mud_dry_diff_4k.jpg
//             if (!rel.empty() && rel.native()[0] != '.')
//                 path = mesh.diffuseMapPath;
//             // map_Kd brown_mud_dry_diff_4k.jpg
//             else
//                 path = "resources" / mesh.diffuseMapPath;
//         }

//         std::string absoluteSourcePath = std::filesystem::absolute(path).string();
//         std::string absoluteResourcesPath = std::filesystem::absolute("resources").string();

//         UTILASSERT(absoluteSourcePath.substr(0, absoluteResourcesPath.length()).compare(absoluteResourcesPath) == 0);

//         TextureBufferAndMemory textureBufferAndMemory = LoadTextureFromFile(path);
//         VkFormat textureFormat = getBestFormatFromChannels(textureBufferAndMemory.channels);

//         textures[0] = CreateImage(
//         textureBufferAndMemory.width, textureBufferAndMemory.height,
//         textureFormat, VK_IMAGE_TILING_OPTIMAL,
//         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
//             );
//         ChangeImageLayout(textures[0].imageAndMemory, 
//                     textureFormat, 
//                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
//                 );
//         CopyBufferToImage(textureBufferAndMemory, textures[0].imageAndMemory);
//         ChangeImageLayout(textures[0].imageAndMemory, 
//                     textureFormat, 
//                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
//                 );
        
//         vkDestroyBuffer(m_EngineDevice, textureBufferAndMemory.bufferAndMemory.buffer, NULL);
//         vkFreeMemory(m_EngineDevice, textureBufferAndMemory.bufferAndMemory.memory, NULL);

//         m_AllocatedBuffers.erase(std::find(m_AllocatedBuffers.begin(), m_AllocatedBuffers.end(), textureBufferAndMemory.bufferAndMemory.buffer));
//         m_AllocatedMemory.erase(std::find(m_AllocatedMemory.begin(), m_AllocatedMemory.end(), textureBufferAndMemory.bufferAndMemory.memory));
//     } else {
//         textures[0] = CreateSinglePixelImage(mesh.diffuse);
//     }

//     return textures;
// }

TextureBufferAndMemory VulkanRenderer::LoadTextureFromFile(const std::string &name) {
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

    BufferAndMemory textureBuffer;

    AllocateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, textureBuffer);

    textureBuffer.size = imageSize;

    m_AllocatedBuffers.push_back(textureBuffer.buffer);
    m_AllocatedMemory.push_back(textureBuffer.memory);

    vkMapMemory(m_EngineDevice, textureBuffer.memory, 0, imageSize, 0, &textureBuffer.mappedData);
    SDL_memcpy(textureBuffer.mappedData, imageData, imageSize);
    vkUnmapMemory(m_EngineDevice, textureBuffer.memory);

    stbi_image_free(imageData);

    return {textureBuffer, (Uint32)texWidth, (Uint32)texHeight, (Uint8)4};
}

VkImageView VulkanRenderer::CreateImageView(TextureImageAndMemory &imageAndMemory, VkFormat format, VkImageAspectFlags aspectMask, bool recordCreation) {
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

VkSampler VulkanRenderer::CreateSampler(float maxAnisotropy, bool recordCreation) {
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

VkFormat VulkanRenderer::FindBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
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

RenderMesh VulkanRenderer::LoadMesh(const Mesh3D &mesh, const Model3D *model) {
    RenderMesh renderMesh{};

    renderMesh.model = model;
    renderMesh.mesh = &mesh;

    BufferAndMemory vertexBuffer = CreateVertexBuffer(mesh.GetVertices());
    renderMesh.vertexBuffer = vertexBuffer;

    renderMesh.indexBufferSize = mesh.GetIndices().size();
    renderMesh.indexBuffer = CreateIndexBuffer(mesh.GetIndices());

    // if (loadTextures) {
    //     std::array<TextureImageAndMemory, 1> meshTextures = LoadTexturesFromMesh(mesh, false);
    //     renderModel.diffTexture = meshTextures[0];

    //     VkFormat textureFormat = getBestFormatFromChannels(renderModel.diffTexture.channels);

    //     // Image view, for sampling.
    //     renderModel.diffTexture.imageAndMemory.view = CreateImageView(renderModel.diffTexture, textureFormat, VK_IMAGE_ASPECT_COLOR_BIT, false);

    //     VkPhysicalDeviceProperties properties{};
    //     vkGetPhysicalDeviceProperties(m_EnginePhysicalDevice, &properties);

    //     renderModel.diffTexture.imageAndMemory.sampler = CreateSampler(properties.limits.maxSamplerAnisotropy, false);
    // }

    renderMesh.diffColor = mesh.GetMaterial().GetColor();

    // MatricesUBO
    {
        VkDeviceSize uniformBufferSize = sizeof(MatricesUBO);

        renderMesh.matricesUBO = {glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};

        AllocateBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderMesh.matricesUBOBuffer);

        vkMapMemory(m_EngineDevice, renderMesh.matricesUBOBuffer.memory, 0, uniformBufferSize, 0, &renderMesh.matricesUBOBuffer.mappedData);

        renderMesh.matricesUBOBuffer.size = uniformBufferSize;
    }

    // MaterialUBO
    {
        VkDeviceSize uniformBufferSize = sizeof(MaterialsUBO);

        renderMesh.materialUBO = {mesh.GetMaterial().GetColor()};

        AllocateBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderMesh.materialsUBOBuffer);

        vkMapMemory(m_EngineDevice, renderMesh.materialsUBOBuffer.memory, 0, uniformBufferSize, 0, &renderMesh.materialsUBOBuffer.mappedData);

        renderMesh.materialsUBOBuffer.size = uniformBufferSize;
    }

    return renderMesh;
}

void VulkanRenderer::SetMouseCaptureState(bool capturing) {
    SDL_SetWindowRelativeMouseMode(m_EngineWindow, capturing);
}

void VulkanRenderer::LoadModel(const Model3D *model) {
    std::vector<std::future<RenderMesh>> tasks;

    for (const Mesh3D &mesh : model->GetMeshes()) {
        tasks.push_back(std::async(std::launch::deferred, &VulkanRenderer::LoadMesh, this, std::ref(mesh), model));
    }

    // Any exception here is going to just happen and get caught like a regular engine error.
    for (std::future<RenderMesh> &task : tasks) {
        m_RenderModels.push_back(task.share().get());
    }

    return;
}

void VulkanRenderer::UnloadRenderModel(RenderMesh &renderModel) {
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

void VulkanRenderer::UnloadModel(const Model3D *model) {
    for (size_t i = 0; i < m_RenderModels.size(); i++) {
        if (m_RenderModels[i].model != model)
            continue;

        RenderMesh renderModel = m_RenderModels[i];

        m_RenderModels.erase(m_RenderModels.begin() + (i--));

        UnloadRenderModel(renderModel);

        // Since now, the Model object is now owned by the caller.
        // delete model; // delete the pointer to the Model object
    }
}

void VulkanRenderer::AddUIChildren(UI::GenericElement *element) {
    for (UI::GenericElement *child : element->GetChildren()) {
        AddUIGenericElement(child);
    }
}

bool VulkanRenderer::RemoveUIChildren(UI::GenericElement *element) {
    for (UI::GenericElement *child : element->GetChildren()) {
        RemoveUIGenericElement(child);
    }

    return true;
}

void VulkanRenderer::AddUIGenericElement(UI::GenericElement *element) {
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

bool VulkanRenderer::RemoveUIGenericElement(UI::GenericElement *element) {
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

void VulkanRenderer::AddUIWaypoint(UI::Waypoint *waypoint) {
    RenderUIWaypoint renderUIWaypoint{};

    renderUIWaypoint.waypoint = waypoint;

    // matrices UBO
    VkDeviceSize matricesUniformBufferSize = sizeof(MatricesUBO);

    renderUIWaypoint.matricesUBO = {glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};

    AllocateBuffer(matricesUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderUIWaypoint.matricesUBOBuffer);

    vkMapMemory(m_EngineDevice, renderUIWaypoint.matricesUBOBuffer.memory, 0, matricesUniformBufferSize, 0, &renderUIWaypoint.matricesUBOBuffer.mappedData);

    renderUIWaypoint.matricesUBOBuffer.size = matricesUniformBufferSize;

    // waypoint UBO
    VkDeviceSize waypointUniformBufferSize = sizeof(UIWaypointUBO);

    renderUIWaypoint.waypointUBO = {waypoint->GetWorldSpacePosition()};

    AllocateBuffer(waypointUniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, renderUIWaypoint.waypointUBOBuffer);

    vkMapMemory(m_EngineDevice, renderUIWaypoint.waypointUBOBuffer.memory, 0, waypointUniformBufferSize, 0, &renderUIWaypoint.waypointUBOBuffer.mappedData);

    renderUIWaypoint.waypointUBOBuffer.size = waypointUniformBufferSize;

    m_RenderUIWaypoints.push_back(renderUIWaypoint);
}

bool VulkanRenderer::RemoveUIWaypoint(UI::Waypoint *waypoint) {
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
    }

    AddUIChildren(waypoint);

    return found;
}

void VulkanRenderer::AddUIPanel(UI::Panel *panel) {
    RenderUIPanel renderUIPanel{};

    renderUIPanel.panel = panel;
    panel->texture.imageAndMemory.view = CreateImageView(panel->texture, panel->texture.format, VK_IMAGE_ASPECT_COLOR_BIT, false);
    panel->texture.imageAndMemory.sampler = CreateSampler(1.0f, false);

    renderUIPanel.ubo.Dimensions = panel->GetDimensions();
    renderUIPanel.ubo.Depth = panel->GetDepth();

    AllocateBuffer(sizeof(renderUIPanel.ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, renderUIPanel.uboBuffer);
    vkMapMemory(m_EngineDevice, renderUIPanel.uboBuffer.memory, 0, sizeof(renderUIPanel.ubo), 0, &(renderUIPanel.uboBuffer.mappedData));

    renderUIPanel.uboBuffer.size = sizeof(renderUIPanel.ubo);

    m_UIPanels.push_back(renderUIPanel);
}

bool VulkanRenderer::RemoveUIPanel(UI::Panel *panel) {
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

void VulkanRenderer::AddUILabel(UI::Label *label) {
    RenderUILabel renderUILabel{};

    renderUILabel.label = label;

    for (auto &glyph : label->Glyphs) {
        auto &glyphBuffer = glyph.glyphBuffer.value();

        glyphBuffer.first.imageAndMemory.view = CreateImageView(glyphBuffer.first, glyphBuffer.first.format, VK_IMAGE_ASPECT_COLOR_BIT, false);
        glyphBuffer.first.imageAndMemory.sampler = CreateSampler(1.0f, false);
    }

    renderUILabel.ubo.PositionOffset = label->GetPosition();
    renderUILabel.ubo.PositionOffset *= 2;

    renderUILabel.ubo.Depth = label->GetDepth();

    AllocateBuffer(sizeof(renderUILabel.ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, renderUILabel.uboBuffer);
    vkMapMemory(m_EngineDevice, renderUILabel.uboBuffer.memory, 0, sizeof(renderUILabel.ubo), 0, &(renderUILabel.uboBuffer.mappedData));

    renderUILabel.uboBuffer.size = sizeof(renderUILabel.ubo);

    m_UILabels.push_back(renderUILabel);
}

bool VulkanRenderer::RemoveUILabel(UI::Label *label) {
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

        for (Glyph &glyph: renderUILabel.label->Glyphs) {
            if (!glyph.glyphBuffer) {
                continue;
            }

            vkDestroyImageView(m_EngineDevice, glyph.glyphBuffer->first.imageAndMemory.view, NULL);
            vkDestroySampler(m_EngineDevice, glyph.glyphBuffer->first.imageAndMemory.sampler, NULL);
        }

        vkDestroyBuffer(m_EngineDevice, renderUILabel.uboBuffer.buffer, NULL);
        vkFreeMemory(m_EngineDevice, renderUILabel.uboBuffer.memory, NULL);

        break;
    }

    return found;
}

Glyph VulkanRenderer::GenerateGlyph(FT_Face ftFace, char c, float &x, float &y, float depth) {
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

                AllocateBuffer(sizeof(glyph.glyphUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyph.glyphUBOBuffer);
                vkMapMemory(m_EngineDevice, glyph.glyphUBOBuffer.memory, 0, sizeof(glyph.glyphUBO), 0, &(glyph.glyphUBOBuffer.mappedData));

                glyph.glyphUBOBuffer.size = sizeof(glyph.glyphUBO);

                return glyph;
            }
    }

    VkDeviceSize glyphBufferSize = static_cast<VkDeviceSize>(ftFace->glyph->bitmap.width * ftFace->glyph->bitmap.rows);

    TextureBufferAndMemory glyphBuffer{};
    AllocateBuffer(glyphBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyphBuffer.bufferAndMemory);
    glyphBuffer.width = ftFace->glyph->bitmap.width;
    glyphBuffer.height = ftFace->glyph->bitmap.rows;
    glyphBuffer.channels = 1;

    vkMapMemory(m_EngineDevice, glyphBuffer.bufferAndMemory.memory, 0, glyphBufferSize, 0, &(glyphBuffer.bufferAndMemory.mappedData));
    SDL_memcpy(glyphBuffer.bufferAndMemory.mappedData, ftFace->glyph->bitmap.buffer, glyphBufferSize);

    glyphBuffer.bufferAndMemory.size = glyphBufferSize;

    TextureImageAndMemory textureImageAndMemory = CreateImage(ftFace->glyph->bitmap.width, ftFace->glyph->bitmap.rows, VK_FORMAT_R8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ChangeImageLayout(textureImageAndMemory.imageAndMemory, 
                VK_FORMAT_R8_SRGB, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
    CopyBufferToImage(glyphBuffer, textureImageAndMemory.imageAndMemory);
    ChangeImageLayout(textureImageAndMemory.imageAndMemory, 
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

    BufferAndMemory bufferAndMemory = CreateSimpleVertexBuffer(simpleVerts);
    
    glyph.offset.x = xpos;
    glyph.offset.y = ypos;

    glyph.scale.x = w;
    glyph.scale.y = h;

    // The bitshift by 6 is required because Advance is 1/64th of a pixel.
    x += ftFace->glyph->advance.x >> 6;

    glyph.glyphBuffer = std::make_pair(textureImageAndMemory, bufferAndMemory);

    AllocateBuffer(sizeof(glyph.glyphUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, glyph.glyphUBOBuffer);
    vkMapMemory(m_EngineDevice, glyph.glyphUBOBuffer.memory, 0, sizeof(glyph.glyphUBO), 0, &(glyph.glyphUBOBuffer.mappedData));

    glyph.glyphUBOBuffer.size = sizeof(glyph.glyphUBO);

    m_GlyphCache.push_back(glyph);

    return glyph;
}

TextureImageAndMemory VulkanRenderer::CreateSinglePixelImage(glm::vec3 color) {
    /* Allocate the buffer that stores our pixel data. */
    BufferAndMemory textureBufferAndMemory;

    VkDeviceSize bufferSize = sizeof(Uint8) * 4;  // R8G8B8A8

    AllocateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, textureBufferAndMemory);

    // copy the texture data into the buffer
    std::array<Uint8, 4> texColors = {static_cast<Uint8>(color.r * 255), static_cast<Uint8>(color.g * 255), static_cast<Uint8>(color.b * 255), 255};

    vkMapMemory(m_EngineDevice, textureBufferAndMemory.memory, 0, bufferSize, 0, &textureBufferAndMemory.mappedData);
    SDL_memcpy(textureBufferAndMemory.mappedData, (void *)texColors.data(), bufferSize);
    vkUnmapMemory(m_EngineDevice, textureBufferAndMemory.memory);

    textureBufferAndMemory.size = bufferSize;

    /* Transfer our newly created texture to an image */
    TextureImageAndMemory textureImageAndMemory = CreateImage(1, 1,
    VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    ChangeImageLayout(textureImageAndMemory.imageAndMemory, 
                VK_FORMAT_R8G8B8A8_SRGB, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
    CopyBufferToImage({textureBufferAndMemory, 1, 1, 4}, textureImageAndMemory.imageAndMemory);
    ChangeImageLayout(textureImageAndMemory.imageAndMemory, 
                VK_FORMAT_R8G8B8A8_SRGB, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
    
    vkDestroyBuffer(m_EngineDevice, textureBufferAndMemory.buffer, NULL);
    vkFreeMemory(m_EngineDevice, textureBufferAndMemory.memory, NULL);

    return textureImageAndMemory;
}

void VulkanRenderer::AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferAndMemory &bufferAndMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_EngineDevice, &bufferInfo, NULL, &bufferAndMemory.buffer) != VK_SUCCESS)
        throw std::runtime_error(engineError::CANT_CREATE_VERTEX_BUFFER);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(m_EngineDevice, bufferAndMemory.buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_EngineDevice, &allocInfo, NULL, &bufferAndMemory.memory) != VK_SUCCESS)
        throw std::runtime_error(engineError::CANT_ALLOCATE_MEMORY);

    vkBindBufferMemory(m_EngineDevice, bufferAndMemory.buffer, bufferAndMemory.memory, 0);
}

VkCommandBuffer VulkanRenderer::BeginSingleTimeCommands() {
    m_SingleTimeCommandMutex.lock();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_EngineDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanRenderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_EngineDevice, m_CommandPool, 1, &commandBuffer);

    m_SingleTimeCommandMutex.unlock();
}

TextureImageAndMemory VulkanRenderer::CreateImage(Uint32 width, Uint32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
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

    if (vkCreateImage(m_EngineDevice, &imageInfo, nullptr, &textureImageAndMemory.imageAndMemory.image) != VK_SUCCESS) {
        throw std::runtime_error(engineError::IMAGE_CREATION_FAILURE);
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_EngineDevice, textureImageAndMemory.imageAndMemory.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_EngineDevice, &allocInfo, nullptr, &textureImageAndMemory.imageAndMemory.memory) != VK_SUCCESS) {
        throw std::runtime_error(engineError::CANT_ALLOCATE_MEMORY);
    }

    textureImageAndMemory.imageAndMemory.size = memRequirements.size;

    textureImageAndMemory.width = width;
    textureImageAndMemory.height = height;
    textureImageAndMemory.channels = getChannelsFromFormats(format);
    textureImageAndMemory.format = format;

    vkBindImageMemory(m_EngineDevice, textureImageAndMemory.imageAndMemory.image, textureImageAndMemory.imageAndMemory.memory, 0);

    return textureImageAndMemory;
}

void VulkanRenderer::ChangeImageLayout(ImageAndMemory &imageAndMemory, VkFormat format, VkImageLayout oldImageLayout, VkImageLayout newImageLayout) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.image = imageAndMemory.image;
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

    EndSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::CopyBufferToImage(TextureBufferAndMemory textureBuffer, ImageAndMemory imageAndMemory) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

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
        imageAndMemory.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        1, &bufferImageCopy
    );

    EndSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::DestroyImage(ImageAndMemory imageAndMemory) {
    vkDestroyImage(m_EngineDevice, imageAndMemory.image, NULL);
    vkFreeMemory(m_EngineDevice, imageAndMemory.memory, NULL);

    auto imageInAllocatedImages = std::find(m_AllocatedImages.begin(), m_AllocatedImages.end(), imageAndMemory.image);
    auto memoryInAllocatedMemory = std::find(m_AllocatedMemory.begin(), m_AllocatedMemory.end(), imageAndMemory.memory);

    if (imageInAllocatedImages != m_AllocatedImages.end()) {
        m_AllocatedImages.erase(imageInAllocatedImages);
    }

    if (memoryInAllocatedMemory != m_AllocatedMemory.end()) {
        m_AllocatedMemory.erase(memoryInAllocatedMemory);
    }
}

BufferAndMemory VulkanRenderer::CreateSimpleVertexBuffer(const std::vector<SimpleVertex> &simpleVerts) {
    //if (m_VertexBuffer || m_VertexBufferMemory)
    //    throw std::runtime_error(engineError::VERTEX_BUFFER_ALREADY_EXISTS);

    // allocate the staging buffer (this is used for performance, having a buffer readable by the GPU and CPU is slower than a GPU-exclusive memory, so we will use that as our real vertex buffer)
    BufferAndMemory stagingBuffer;

    VkDeviceSize stagingBufferSize = sizeof(SimpleVertex) * simpleVerts.size();

    AllocateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

    // copy the vertex data into the buffer
    vkMapMemory(m_EngineDevice, stagingBuffer.memory, 0, stagingBufferSize, 0, &stagingBuffer.mappedData);
    SDL_memcpy(stagingBuffer.mappedData, (void *)simpleVerts.data(), stagingBufferSize);
    vkUnmapMemory(m_EngineDevice, stagingBuffer.memory);

    // allocate the gpu-exclusive vertex buffer
    BufferAndMemory vertexBuffer;

    AllocateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer);

    CopyHostBufferToDeviceBuffer(stagingBuffer.buffer, vertexBuffer.buffer, stagingBufferSize);

    vertexBuffer.size = stagingBufferSize;

    vkDestroyBuffer(m_EngineDevice, stagingBuffer.buffer, NULL);
    vkFreeMemory(m_EngineDevice, stagingBuffer.memory, NULL);

    return vertexBuffer;
}


BufferAndMemory VulkanRenderer::CreateVertexBuffer(const std::vector<Vertex> &verts) {
    //if (m_VertexBuffer || m_VertexBufferMemory)
    //    throw std::runtime_error(engineError::VERTEX_BUFFER_ALREADY_EXISTS);

    // allocate the staging buffer (this is used for performance, having a buffer readable by the GPU and CPU is slower than a GPU-exclusive memory, so we will use that as our real vertex buffer)
    BufferAndMemory stagingBuffer;

    VkDeviceSize stagingBufferSize = sizeof(Vertex) * verts.size();

    AllocateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

    // copy the vertex data into the buffer
    vkMapMemory(m_EngineDevice, stagingBuffer.memory, 0, stagingBufferSize, 0, &stagingBuffer.mappedData);
    SDL_memcpy(stagingBuffer.mappedData, (void *)verts.data(), stagingBufferSize);
    vkUnmapMemory(m_EngineDevice, stagingBuffer.memory);

    // allocate the gpu-exclusive vertex buffer
    BufferAndMemory vertexBuffer;

    AllocateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer);

    CopyHostBufferToDeviceBuffer(stagingBuffer.buffer, vertexBuffer.buffer, stagingBufferSize);

    vertexBuffer.size = stagingBufferSize;

    vkDestroyBuffer(m_EngineDevice, stagingBuffer.buffer, NULL);
    vkFreeMemory(m_EngineDevice, stagingBuffer.memory, NULL);

    return vertexBuffer;
}

BufferAndMemory VulkanRenderer::CreateIndexBuffer(const std::vector<Uint32> &inds) {
    //if (m_IndexBuffer || m_IndexBufferMemory)
    //    throw std::runtime_error(engineError::INDEX_BUFFER_ALREADY_EXISTS);

    // allocate the staging buffer (this is used for performance, having a buffer readable by the GPU and CPU is slower than a GPU-exclusive memory, so we will use that as our real index buffer)
    BufferAndMemory stagingBuffer;

    VkDeviceSize stagingBufferSize = sizeof(Uint32) * inds.size();
    AllocateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

    // copy the index data into the buffer
    vkMapMemory(m_EngineDevice, stagingBuffer.memory, 0, stagingBufferSize, 0, &stagingBuffer.mappedData);
    SDL_memcpy(stagingBuffer.mappedData, (void *)inds.data(), stagingBufferSize);
    vkUnmapMemory(m_EngineDevice, stagingBuffer.memory);

    // allocate the gpu-exclusive index buffer
    BufferAndMemory indexBuffer;

    AllocateBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer);

    CopyHostBufferToDeviceBuffer(stagingBuffer.buffer, indexBuffer.buffer, stagingBufferSize);

    indexBuffer.size = stagingBufferSize;

    vkDestroyBuffer(m_EngineDevice, stagingBuffer.buffer, NULL);
    vkFreeMemory(m_EngineDevice, stagingBuffer.memory, NULL);

    return indexBuffer;
}

void VulkanRenderer::BeginRenderPass(RenderPass *renderPass, std::any framebuffer) {
    glm::vec2 renderPassResolution = renderPass->GetResolution();

    // begin a render pass, this could be for example HDR pass, SSAO pass, lighting pass.
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = std::any_cast<VkRenderPass>(renderPass->GetRawRenderPass());
    renderPassBeginInfo.framebuffer = std::any_cast<VkFramebuffer>(framebuffer);
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = {static_cast<uint32_t>(renderPassResolution.x), static_cast<uint32_t>(renderPassResolution.y)};

    std::array<VkClearValue, 2> clearColors{};
    glm::vec4 renderPassClearColors = renderPass->GetClearColor();

    clearColors[0].color = {{renderPassClearColors.r, renderPassClearColors.g, renderPassClearColors.b, renderPassClearColors.a}};
    clearColors[1].depthStencil = {1.0f, 0};

    renderPassBeginInfo.clearValueCount = clearColors.size();
    renderPassBeginInfo.pClearValues = clearColors.data();

    /* This should work under normal cases, but I don't feel too good about using m_FrameIndex, this feels like it could be better.. */
    vkCmdBeginRenderPass(m_CommandBuffers[m_FrameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::StartNextSubpass() {
    vkCmdNextSubpass(m_CommandBuffers[m_FrameIndex], VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::BeginPipeline(GraphicsPipeline *pipeline) {
    vkCmdBindPipeline(m_CommandBuffers[m_FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, std::any_cast<VkPipeline>(pipeline->GetRawPipeline()));

    glm::vec4 pipelineViewport = pipeline->GetViewport();
    m_PipelineViewport = {pipelineViewport.x, pipelineViewport.y, pipelineViewport.z, pipelineViewport.w, 0.0f, 1.0f};
    vkCmdSetViewport(m_CommandBuffers[m_FrameIndex], 0, 1, &m_PipelineViewport);

    glm::vec4 pipelineScissor = pipeline->GetScissor();
    m_PipelineScissor = {
        {static_cast<int32_t>(pipelineScissor.x), static_cast<int32_t>(pipelineScissor.y)},
        {static_cast<uint32_t>(pipelineScissor.z), static_cast<uint32_t>(pipelineScissor.w)}
    };
    
    vkCmdSetScissor(m_CommandBuffers[m_FrameIndex], 0, 1, &m_PipelineScissor);
}

void VulkanRenderer::Draw(GraphicsPipeline *pipeline, BufferAndMemory vertexBuffer, Uint32 vertexCount, std::optional<BufferAndMemory> indexBuffer, Uint32 indexCount) {
    // glm::mat4 viewMatrix;
    // glm::mat4 projectionMatrix;

    // if (m_PrimaryCamera) {
    //     viewMatrix = m_PrimaryCamera->GetViewMatrix();

    //     if (m_PrimaryCamera->type == CAMERA_PERSPECTIVE) {
    //         projectionMatrix = glm::perspective(glm::radians(m_PrimaryCamera->FOV), (float)m_Settings.RenderWidth / (float)m_Settings.RenderHeight, m_Settings.CameraNear, CAMERA_FAR);
    //     } else {
    //         projectionMatrix = glm::ortho(0.0f, m_PrimaryCamera->OrthographicWidth, 0.0f, m_PrimaryCamera->OrthographicWidth*m_PrimaryCamera->AspectRatio);
    //     }

    //     // invert Y axis, glm was meant for OpenGL which inverts the Y axis.
    //     projectionMatrix[1][1] *= -1;

    //     for (RenderModel &renderModel : m_RenderModels) {
    //         renderModel.matricesUBO.modelMatrix = renderModel.model->GetModelMatrix();

    //         renderModel.matricesUBO.viewMatrix = viewMatrix;
    //         renderModel.matricesUBO.projectionMatrix = projectionMatrix;

    //         SDL_memcpy(renderModel.matricesUBOBuffer.mappedData, &renderModel.matricesUBO, sizeof(renderModel.matricesUBO));

    // vertex buffer binding!!
    VkDeviceSize mainOffsets[] = {0};
    vkCmdBindVertexBuffers(m_CommandBuffers[m_FrameIndex], 0, 1, &vertexBuffer.buffer, mainOffsets);

    if (indexBuffer.has_value()) {
        vkCmdBindIndexBuffer(m_CommandBuffers[m_FrameIndex], indexBuffer.value().buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    std::vector<PipelineBinding> pipelineBindings = pipeline->GetDescriptorLayout().GetBindings();

    std::vector<VkDescriptorBufferInfo> bufferInfos(pipelineBindings.size());
    std::vector<VkDescriptorImageInfo> imageInfos(pipelineBindings.size());
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    for (PipelineBinding &binding : pipelineBindings) {
        BufferAndMemory bufferAndMemory;
        ImageAndMemory imageAndMemory;

        VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back();
        VkDescriptorImageInfo &imageInfo = imageInfos.emplace_back();

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.pNext = nullptr;
        descriptorWrite.dstSet = 0x0; // Ignored
        descriptorWrite.dstBinding = binding.bindingIndex;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = binding.type;
        descriptorWrite.descriptorCount = 1;

        switch (binding.type) {
            /* TODO: implement all */
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                bufferAndMemory = std::any_cast<BufferAndMemory>(pipeline->GetBindingValue(binding.bindingIndex));

                bufferInfo.buffer = bufferAndMemory.buffer;
                bufferInfo.offset = 0;
                bufferInfo.range = bufferAndMemory.size;

                descriptorWrite.pBufferInfo = &bufferInfo;

                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                imageAndMemory = std::any_cast<ImageAndMemory>(pipeline->GetBindingValue(binding.bindingIndex));

                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = imageAndMemory.view;
                imageInfo.sampler = imageAndMemory.sampler;

                descriptorWrite.pImageInfo = &imageInfo;

                break;
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
            case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
            case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
            case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
            case VK_DESCRIPTOR_TYPE_MAX_ENUM:
                break;
        }

        descriptorWrites.push_back(descriptorWrite);
    }

    vkCmdPushDescriptorSet(m_CommandBuffers[m_FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, std::any_cast<VkPipelineLayout>(pipeline->GetRawPipelineLayout()), 0, descriptorWrites.size(), descriptorWrites.data());

    if (indexBuffer.has_value()) {
        vkCmdDrawIndexed(m_CommandBuffers[m_FrameIndex], indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(m_CommandBuffers[m_FrameIndex], vertexCount, 1, 0, 0);
    }
}

void VulkanRenderer::EndRenderPass() {
    /* This should work under normal cases, but I don't feel too good about using m_FrameIndex, this feels like it could be better.. */
    vkCmdEndRenderPass(m_CommandBuffers[m_FrameIndex]);
}

std::any VulkanRenderer::CreateDescriptorSetLayout(std::vector<PipelineBinding> &pipelineBindings) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (PipelineBinding &binding : pipelineBindings) {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.binding = binding.bindingIndex;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.descriptorType = binding.type;
        descriptorSetLayoutBinding.stageFlags = binding.shaderStageBits;
        descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

        bindings.push_back(descriptorSetLayoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
    descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
    descriptorSetLayoutCreateInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;

    if (vkCreateDescriptorSetLayout(m_EngineDevice, &descriptorSetLayoutCreateInfo, NULL, &layout) != VK_SUCCESS)
        throw std::runtime_error(engineError::DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE);

    m_AllocatedDescriptorSetLayouts.push_back(layout);

    return layout;
}

void VulkanRenderer::InitSwapchain() {
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

void VulkanRenderer::InitFramebuffers(RenderPass *renderPass, VkImageView depthImageView) {
    for (VkFramebuffer framebuffer : m_SwapchainFramebuffers)
        if (framebuffer)
            vkDestroyFramebuffer(m_EngineDevice, framebuffer, NULL);

    m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

    for (size_t i = 0; i < m_SwapchainFramebuffers.size(); i++)
        m_SwapchainFramebuffers[i] = CreateFramebuffer(renderPass, m_SwapchainImageViews[i], depthImageView);
}

VkImageView VulkanRenderer::CreateDepthImage(Uint32 width, Uint32 height) {
    VkFormat depthFormat = FindDepthFormat();

    TextureImageAndMemory depthImageAndMemory = CreateImage(width, height, 
                        depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkImageView depthImageView = CreateImageView(depthImageAndMemory, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    m_AllocatedImages.push_back(depthImageAndMemory.imageAndMemory.image);
    m_AllocatedMemory.push_back(depthImageAndMemory.imageAndMemory.memory);

    return depthImageView;
}

Uint32 VulkanRenderer::FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_EnginePhysicalDevice, &memoryProperties);

    for (Uint32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error(engineError::CANT_FIND_SUITABLE_MEMTYPE);
}

void VulkanRenderer::InitInstance() {
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

/*
 * Creates a Vulkan graphics pipeline, shaderName will be used as a part of the path.
 * Sanitization is the job of the caller.
 */
GraphicsPipeline *VulkanRenderer::CreateGraphicsPipeline(const std::vector<Shader> &shaders, RenderPass *renderPass, Uint32 subpassIndex, VkFrontFace frontFace, glm::vec4 viewport, glm::vec4 scissor, const DescriptorLayout &descriptorSetLayout, bool isSimple, bool enableDepth) {
    PipelineAndLayout pipelineAndLayout;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    for (const Shader &shader : shaders) {
        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = shader.GetShaderStageBits();
        shaderStageInfo.module = std::any_cast<VkShaderModule>(shader.GetShaderModule());
        shaderStageInfo.pName = "main";

        shaderStages.push_back(shaderStageInfo);
    }

    VkDescriptorSetLayout descriptorSetLayoutRaw = std::any_cast<VkDescriptorSetLayout>(descriptorSetLayout.Get());

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayoutRaw;
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

    /* TODO: Allow for dynamic vertex binding descriptions. */
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

    VkViewport vkViewport{viewport.x, viewport.y, viewport.z, viewport.w, 0.0f, 1.0f};
    VkRect2D vkScissor{{static_cast<int32_t>(scissor.x), static_cast<int32_t>(scissor.y)}, 
        {static_cast<uint32_t>(scissor.z), static_cast<uint32_t>(scissor.w)}
    };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &vkViewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &vkScissor;

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
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
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
    pipelineInfo.renderPass = std::any_cast<VkRenderPass>(renderPass->GetRawRenderPass());
    pipelineInfo.subpass = subpassIndex;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m_EngineDevice, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipelineAndLayout.pipeline) != VK_SUCCESS) {
        for (const Shader &shader : shaders) {
            DestroyShaderModule(shader.GetShaderModule());
        }

        throw std::runtime_error(engineError::PIPELINE_CREATION_FAILURE);
    }

    for (const Shader &shader : shaders) {
        DestroyShaderModule(shader.GetShaderModule());
    }

    GraphicsPipeline *pipeline = new GraphicsPipeline(pipelineAndLayout.pipeline, pipelineAndLayout.layout, descriptorSetLayout, this, viewport, scissor);

    renderPass->SetSubpass(subpassIndex, pipeline);
    m_Pipelines.push_back(pipeline);

    return pipeline;
}

RenderPass *VulkanRenderer::CreateRenderPass(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, size_t subpassCount, VkFormat imageFormat, VkImageLayout initialColorLayout, VkImageLayout finalColorLayout, glm::vec2 resolution, bool shouldContainDepthImage) {
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

    VkRenderPass vulkanRenderPass;
    if (vkCreateRenderPass(m_EngineDevice, &renderPassInfo, NULL, &vulkanRenderPass))
        throw std::runtime_error(engineError::RENDERPASS_CREATION_FAILURE);

    RenderPass *renderPass = new RenderPass(this, vulkanRenderPass, resolution);

    m_RenderPasses.push_back(renderPass);

    return renderPass;
}

VkFramebuffer VulkanRenderer::CreateFramebuffer(RenderPass *renderPass, VkImageView imageView, VkImageView depthImageView) {
    std::array<VkImageView, 2> attachments = {imageView, depthImageView};

    glm::vec2 resolution = renderPass->GetResolution();

    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = std::any_cast<VkRenderPass>(renderPass->GetRawRenderPass());
    createInfo.attachmentCount = 1 + (depthImageView != nullptr);   // Bools get converted to ints, This will be 1 if depthImageView == nullptr, 2 if it isn't.
    createInfo.pAttachments = attachments.data();
    createInfo.width = resolution.x;
    createInfo.height = resolution.y;
    createInfo.layers = 1;

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(m_EngineDevice, &createInfo, NULL, &framebuffer) != VK_SUCCESS)
        throw std::runtime_error(engineError::FRAMEBUFFER_CREATION_FAILURE);
    
    return framebuffer;
}

void VulkanRenderer::Init() {
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

    m_MainRenderPass = CreateRenderPass(
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
        3, m_RenderImageFormat, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        {m_Settings.RenderWidth, m_Settings.RenderHeight}
    );

    m_RescaleRenderPass = CreateRenderPass(
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
        3, m_SwapchainImageFormat, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        {m_Settings.DisplayWidth, m_Settings.DisplayHeight}
    );

    VkImageView depthImageView = CreateDepthImage(m_Settings.RenderWidth, m_Settings.RenderHeight);
    VkImageView rescaleDepthImageView = CreateDepthImage(m_Settings.DisplayWidth, m_Settings.DisplayHeight);

    TextureImageAndMemory renderImage = CreateImage(m_Settings.RenderWidth, m_Settings.RenderHeight, m_RenderImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    renderImage.imageAndMemory.view = CreateImageView(renderImage, m_RenderImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

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

    m_RenderFramebuffer = CreateFramebuffer(m_MainRenderPass, renderImage.imageAndMemory.view, depthImageView);
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

    // Image view, for sampling the render texture for rescaling.
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_EnginePhysicalDevice, &properties);

    m_RenderImageAndMemory.sampler = CreateSampler(properties.limits.maxSamplerAnisotropy);
        
    // Fullscreen Quad initialization
    m_FullscreenQuadVertexBuffer = CreateSimpleVertexBuffer({
                                                        {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
                                                        {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
                                                        {glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},
                                                        {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
                                                        {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
                                                        {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)}
                                                    });

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

    
    // LightsUBO
    {
        VkDeviceSize uniformBufferSize = sizeof(LightsUBO);

        AllocateBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_LightsUBOBuffer);

        vkMapMemory(m_EngineDevice, m_LightsUBOBuffer.memory, 0, uniformBufferSize, 0, &m_LightsUBOBuffer.mappedData);

        m_LightsUBOBuffer.size = uniformBufferSize;
    }
}

void VulkanRenderer::StepRender() {
    // we got (MAX_FRAMES_IN_FLIGHT) "slots" to use, we can write frames as long as the current frame slot we're using isn't occupied.
    VkResult waitForFencesResult = vkWaitForFences(m_EngineDevice, 1, &m_InFlightFences[m_FrameIndex], true, UINT64_MAX);

    if (waitForFencesResult != VK_SUCCESS) {
        throw std::runtime_error(fmt::format(engineError::WAIT_FOR_FENCES_FAILED, string_VkResult(waitForFencesResult)));
    }

    Uint32 imageIndex = 0;
    VkResult acquireNextImageResult = vkAcquireNextImageKHR(m_EngineDevice, m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_FrameIndex], VK_NULL_HANDLE, &imageIndex);
    
    // the swapchain can become "out of date" if the user were to, say, resize the window.
    // suboptimal means it is kind of out of date but not invalid, can still be used.
    if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        /* TODO: create a function so we don't repeat ourselves. */
        InitSwapchain();


        VkImageView rescaleDepthImageView = CreateDepthImage(m_Settings.DisplayWidth, m_Settings.DisplayHeight);

        // VkImageView depthImageView = CreateDepthImage();

        // TextureImageAndMemory renderImage = CreateImage(m_Settings.RenderWidth, m_Settings.RenderHeight, m_RenderImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        // VkImageView renderImageView = CreateImageView(renderImage, m_RenderImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // m_RenderFramebuffer = CreateFramebuffer(m_MainRenderPass, renderImageView, {m_Settings.RenderWidth, m_Settings.RenderHeight}, depthImageView);
        InitFramebuffers(m_RescaleRenderPass, rescaleDepthImageView);

        return;
    } else if (acquireNextImageResult != VK_SUCCESS)
        throw std::runtime_error(engineError::CANT_ACQUIRE_NEXT_IMAGE);

    // "ight im available, if i wasn't already"
    vkResetFences(m_EngineDevice, 1, &m_InFlightFences[m_FrameIndex]);

    vkResetCommandBuffer(m_CommandBuffers[m_FrameIndex], 0);

    // begin recording my commands
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(m_CommandBuffers[m_FrameIndex], &commandBufferBeginInfo) != VK_SUCCESS)
        throw std::runtime_error(engineError::COMMAND_BUFFER_BEGIN_FAILURE);

    m_MainRenderPass->Execute(m_RenderFramebuffer);
    m_RescaleRenderPass->Execute(m_SwapchainFramebuffers[imageIndex]);

    if (vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]) != VK_SUCCESS)
        throw std::runtime_error(engineError::COMMAND_BUFFER_END_FAILURE);

    // we recorded all the commands, submit them.
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_FrameIndex];
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_FrameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphores[m_FrameIndex];

    VkResult queueSubmitResult = vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_FrameIndex]);
    if (queueSubmitResult != VK_SUCCESS)
        throw std::runtime_error(fmt::format(engineError::QUEUE_SUBMIT_FAILURE, string_VkResult(queueSubmitResult)));

    // we finished, now we should present the frame.
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_FrameIndex];

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;

    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pResults = nullptr;

    vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

    m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    //fixedUpdateThread.join();
}
