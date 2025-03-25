#pragma once
#include "common.hpp"
#include "error.hpp"
#include "renderer/GraphicsPipeline.hpp"
#include "renderer/DescriptorLayout.hpp"
#include "renderer/RenderPass.hpp"
#include "renderer/Shader.hpp"
#include "renderer/baseRenderer.hpp"
#include <freetype/freetype.h>
#include <vulkan/vulkan_core.h>

class VulkanRenderer : public BaseRenderer {
public:
    VulkanRenderer(Settings &settings) : BaseRenderer(settings) {};
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

    std::any CreateShaderModule(const std::vector<std::byte> &code) override;
    void DestroyShaderModule(std::any shaderModule) override;
    
    std::any CreateDescriptorLayout(std::vector<PipelineBinding> &pipelineBindings) override;
    
    void AllocateBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferAndMemory &bufferAndMemory) override;

    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    TextureImageAndMemory CreateImage(Uint32 width, Uint32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) override;

    void ChangeImageLayout(ImageAndMemory &imageAndMemory, VkFormat format, VkImageLayout oldImageLayout, VkImageLayout newImageLayout);
    void CopyBufferToImage(TextureBufferAndMemory textureBuffer, ImageAndMemory imageAndMemory) override;

    void DestroyImage(ImageAndMemory imageAndMemory) override;
    
    BufferAndMemory CreateSimpleVertexBuffer(const std::vector<SimpleVertex> &simpleVerts) override;
    BufferAndMemory CreateVertexBuffer(const std::vector<Vertex> &verts) override;
    BufferAndMemory CreateIndexBuffer(const std::vector<Uint32> &inds) override;

    void BeginRenderPass(RenderPass *renderPass, std::any framebuffer) override;
    void StartNextSubpass() override;
    void BeginPipeline(GraphicsPipeline *pipeline) override;
    void Draw(GraphicsPipeline *pipeline, BufferAndMemory vertexBuffer, Uint32 vertexCount, std::optional<BufferAndMemory> indexBuffer = {}, Uint32 indexCount = 0) override;
    void EndRenderPass() override;

    std::any CreateDescriptorSetLayout(std::vector<PipelineBinding> &pipelineBindings);

    /* TODO: This isn't general enough to make it to BaseRenderer */
    GraphicsPipeline *CreateGraphicsPipeline(const std::vector<Shader> &shaders, RenderPass *renderPass, Uint32 subpassIndex, VkFrontFace frontFace, glm::vec4 viewport, glm::vec4 scissor, const DescriptorLayout &descriptorSetLayout, bool isSimple = VK_TRUE, bool enableDepth = VK_TRUE);

    void Init() override;

    void StepRender() override;
protected:
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSet;

    void MainRenderFunction(GraphicsPipeline *pipeline) {
        glm::mat4 viewMatrix, projectionMatrix;

        if (m_PrimaryCamera) {
            viewMatrix = m_PrimaryCamera->GetViewMatrix();

            if (m_PrimaryCamera->type == CAMERA_PERSPECTIVE) {
                projectionMatrix = glm::perspective(
                    glm::radians(m_PrimaryCamera->FOV), (float)m_Settings.RenderWidth / (float)m_Settings.RenderHeight, m_Settings.CameraNear, CAMERA_FAR
                );
            } else {
                projectionMatrix = glm::ortho(0.0f, m_PrimaryCamera->OrthographicWidth, 0.0f, m_PrimaryCamera->OrthographicWidth*m_PrimaryCamera->AspectRatio);
            }

            // invert Y axis, glm was meant for OpenGL which inverts the Y axis.
            projectionMatrix[1][1] *= -1;

            for (RenderModel &renderModel : m_RenderModels) {
                renderModel.matricesUBO.modelMatrix = renderModel.model->GetModelMatrix();

                renderModel.matricesUBO.viewMatrix = viewMatrix;
                renderModel.matricesUBO.projectionMatrix = projectionMatrix;

                SDL_memcpy(renderModel.matricesUBOBuffer.mappedData, &renderModel.matricesUBO, sizeof(renderModel.matricesUBO));

                pipeline->UpdateBindingValue(0, renderModel.matricesUBOBuffer);
                pipeline->UpdateBindingValue(1, renderModel.diffTexture.imageAndMemory);

                Draw(pipeline, renderModel.vertexBuffer, 0, renderModel.indexBuffer, renderModel.indexBufferSize);
            }
        }
    }
    void UIWaypointRenderFunction(GraphicsPipeline *pipeline) {
        glm::mat4 viewMatrix, projectionMatrix;

        if (m_PrimaryCamera) {
            viewMatrix = m_PrimaryCamera->GetViewMatrix();

            if (m_PrimaryCamera->type == CAMERA_PERSPECTIVE) {
                projectionMatrix = glm::perspective(glm::radians(m_PrimaryCamera->FOV), (float)m_Settings.RenderWidth / (float)m_Settings.RenderHeight, m_Settings.CameraNear, CAMERA_FAR);
            } else {
                projectionMatrix = glm::ortho(0.0f, m_PrimaryCamera->OrthographicWidth, 0.0f, m_PrimaryCamera->OrthographicWidth*m_PrimaryCamera->AspectRatio);
            }

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

                pipeline->UpdateBindingValue(0, renderUIWaypoint.matricesUBOBuffer);
                pipeline->UpdateBindingValue(1, renderUIWaypoint.waypointUBOBuffer);

                Draw(pipeline, m_FullscreenQuadVertexBuffer, 6);
            }
        }
    }
    void UIArrowsRenderFunction(GraphicsPipeline *pipeline) {
        glm::mat4 viewMatrix, projectionMatrix;

        if (m_PrimaryCamera) {
            viewMatrix = m_PrimaryCamera->GetViewMatrix();

            if (m_PrimaryCamera->type == CAMERA_PERSPECTIVE) {
                projectionMatrix = glm::perspective(glm::radians(m_PrimaryCamera->FOV), (float)m_Settings.RenderWidth / (float)m_Settings.RenderHeight, m_Settings.CameraNear, CAMERA_FAR);
            } else {
                projectionMatrix = glm::ortho(0.0f, m_PrimaryCamera->OrthographicWidth, 0.0f, m_PrimaryCamera->OrthographicWidth*m_PrimaryCamera->AspectRatio);
            }
            
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

                    pipeline->UpdateBindingValue(0, matricesUBOBuffer);
                    pipeline->UpdateBindingValue(1, arrowsUBOBuffer);

                    Draw(pipeline, arrowRenderModel.vertexBuffer, 0, arrowRenderModel.indexBuffer, arrowRenderModel.indexBufferSize);

                    i++;
                }
            }
        }
    }
    void RescaleRenderFunction(GraphicsPipeline *pipeline) {
        pipeline->UpdateBindingValue(0, m_RenderImageAndMemory);

        Draw(pipeline, m_FullscreenQuadVertexBuffer, 6);
    }
    void UIPanelRenderFunction(GraphicsPipeline *pipeline) {
        for (RenderUIPanel &renderUIPanel : m_UIPanels) {
            if (!renderUIPanel.panel->GetVisible()) {
                continue;
            }

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

            pipeline->UpdateBindingValue(0, renderUIPanel.uboBuffer);
            pipeline->UpdateBindingValue(1, renderUIPanel.panel->texture.imageAndMemory);                          

            Draw(pipeline, m_FullscreenQuadVertexBuffer, 6);
        }
    }
    void UILabelRenderFunction(GraphicsPipeline *pipeline) {
        for (RenderUILabel &renderUILabel : m_UILabels) {
            if (!renderUILabel.label->GetVisible()) {
                continue;
            }

            renderUILabel.ubo.PositionOffset = renderUILabel.label->GetPosition();
            renderUILabel.ubo.PositionOffset.x *= 2;
            renderUILabel.ubo.PositionOffset.y *= 2;

            renderUILabel.ubo.Depth = renderUILabel.label->GetDepth();

            SDL_memcpy(renderUILabel.uboBuffer.mappedData, &(renderUILabel.ubo), sizeof(renderUILabel.ubo));

            for (Glyph &glyph : renderUILabel.label->Glyphs) {
                glyph.glyphUBO.Offset = glyph.offset;
                
                SDL_memcpy(glyph.glyphUBOBuffer.mappedData, &(glyph.glyphUBO), sizeof(glyph.glyphUBO));

                pipeline->UpdateBindingValue(0, renderUILabel.uboBuffer);
                pipeline->UpdateBindingValue(1, glyph.glyphBuffer->first.imageAndMemory);
                pipeline->UpdateBindingValue(2, glyph.glyphUBOBuffer);

                Draw(pipeline, glyph.glyphBuffer.value().second, 6);
            }
        }
    }

    void InitInstance();
    void InitSwapchain();
    void InitFramebuffers(RenderPass *renderPass, VkImageView depthImageView);
    VkImageView CreateDepthImage(Uint32 width, Uint32 height);
    RenderPass *CreateRenderPass(VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, size_t subpassCount, VkFormat imageFormat, VkImageLayout initialColorLayout, VkImageLayout finalColorLayout, glm::vec2 resolution, bool shouldContainDepthImage = true);
    VkFramebuffer CreateFramebuffer(RenderPass *renderPass, VkImageView imageView, VkImageView depthImageView = nullptr);

    void UnloadRenderModel(RenderModel &renderModel) override;

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
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

    VkDescriptorSetLayout m_UIPanelDescriptorSetLayout = nullptr;
    VkDescriptorSetLayout m_UILabelDescriptorSetLayout = nullptr;

    VkDescriptorSetLayout m_RescaleDescriptorSetLayout = nullptr;

    GraphicsPipeline *m_MainGraphicsPipeline; // Used to render the 3D scene
    GraphicsPipeline *m_UIWaypointGraphicsPipeline; // Used to add shiny waypoints
    GraphicsPipeline *m_UIArrowsGraphicsPipeline; // Used for UI arrows, commonly used in the Map Editor (WIP at the time of writing this comment).
    GraphicsPipeline *m_RescaleGraphicsPipeline; // Used to rescale.
    GraphicsPipeline *m_UIPanelGraphicsPipeline; // Used for UI Panels.
    GraphicsPipeline *m_UILabelGraphicsPipeline; // Used for UI Labels.

    BufferAndMemory m_FullscreenQuadVertexBuffer = {nullptr, nullptr, 0};

    VkSwapchainKHR m_Swapchain = nullptr;
    std::vector<RenderPass *> m_RenderPasses;
    std::vector<GraphicsPipeline *> m_Pipelines;

    RenderPass *m_MainRenderPass;
    VkFramebuffer m_RenderFramebuffer;
    ImageAndMemory m_RenderImageAndMemory;
    VkFormat m_RenderImageFormat;

    RenderPass *m_RescaleRenderPass;   // This uses the swapchain framebuffers

    /* To avoid object deletion, these are members. */
    VkViewport m_PipelineViewport;
    VkRect2D m_PipelineScissor;

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

    // memory cleanup related, will not include any buffer that is already above (e.g. m_VertexBuffer)
    std::vector<VkImage> m_AllocatedImages;
    std::vector<VkBuffer> m_AllocatedBuffers;
    std::vector<VkDeviceMemory> m_AllocatedMemory;
    std::vector<VkImageView> m_CreatedImageViews;
    std::vector<VkSampler> m_CreatedSamplers;
};

inline Uint8 getChannelsFromFormats(VkFormat format) {
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
            return 0; // [-Wreturn-type]
    }
}
