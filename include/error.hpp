#ifndef _ERROR_HPP_
#define _ERROR_HPP_
#include <string>

namespace engineError {
    inline std::string FAILED_SDL_INIT = "Failed to initialize SDL! (code: {})";
    inline std::string FAILED_WINDOW_INIT = "Failed to create window! (error: {})";
    inline std::string FAILED_VULKAN_LOAD = "Failed to load vulkan!";
    inline std::string FAILED_VULKAN_EXTS = "Failed to get extensions for vulkan!";
    inline std::string NO_VULKAN_DEVICES = "Failed to get vulkan capable devices!";
    inline std::string NO_QUEUE_FAMILIES = "Failed to get queue families!";
    inline std::string CANT_CREATE_DEVICE = "Failed to create logical device!";
    inline std::string NO_CAPABLE_CARD = "Failed to find a capable card!";
    inline std::string SWAPCHAIN_INIT_FAILURE = "Failed to create a swapchain! (Error: {})";
    inline std::string IMAGE_VIEW_CREATION_FAILURE = "Failed to create an image view!";
    inline std::string RENDERPASS_CREATION_FAILURE = "Failed to create a render pass!";
    inline std::string PIPELINE_CREATION_FAILURE = "Failed to create a graphics pipeline!";
    inline std::string PIPELINE_LAYOUT_CREATION_FAILURE = "Failed to create a graphics pipeline layout!";
    inline std::string FRAMEBUFFER_CREATION_FAILURE = "Failed to create framebuffers!";
    inline std::string COMMAND_POOL_CREATION_FAILURE = "Failed to create command pool!";
    inline std::string COMMAND_BUFFER_ALLOCATION_FAILURE = "Failed to allocate command buffer!";
    inline std::string COMMAND_BUFFER_BEGIN_FAILURE = "Failed to begin recording the command buffer!";
    inline std::string COMMAND_BUFFER_END_FAILURE = "Failed to end recording of the command buffer!";
    inline std::string SYNC_OBJECTS_CREATION_FAILURE = "Failed to create sync objects!";
    inline std::string QUEUE_SUBMIT_FAILURE = "Failed to submit command buffer to queue! {}";
    inline std::string INSTANCE_CREATION_FAILURE = "Failed to create vulkan instance!";
    inline std::string CANT_ACQUIRE_NEXT_IMAGE = "Failed to acquire next swapchain image!";
    inline std::string CANT_CREATE_VERTEX_BUFFER = "Failed to create a vertex buffer!";
    inline std::string CANT_FIND_SUITABLE_MEMTYPE = "Failed to find a suitable memory type!";
    inline std::string CANT_ALLOCATE_MEMORY = "Failed to allocate device memory!";
    inline std::string VERTEX_BUFFER_ALREADY_EXISTS = "Tried to create a vertex buffer more than twice!";
    inline std::string INDEX_BUFFER_ALREADY_EXISTS = "Tried to create an index buffer more than twice!";
    inline std::string DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE = "Failed to create descriptor set layout!";
    inline std::string TEXTURE_LOADING_FAILURE = "Failed to load texture file, Reason: {}, File: ({})!";
    inline std::string IMAGE_CREATION_FAILURE = "Failed to create image!";
    inline std::string UNSUPPORTED_LAYOUT_TRANSITION = "Unsupported image layout transition!";
    inline std::string SAMPLER_CREATION_FAILURE = "Failed to create sampler!";
    inline std::string INVALID_CHANNEL_COUNT = "Attempted to convert invalid channel count to VkFormat!";
    inline std::string UNSUPPORTED_FORMAT = "Attempted to convert unsupported VkFormat to channel count!";
    inline std::string CANT_FIND_ANY_FORMAT = "Tried to find best format, but none can be used!";
    inline std::string RENDERPASS_PIPELINE_EXISTS = "Tried to create a graphics pipeline for a renderpass that already has a graphics pipeline!";
    inline std::string SURFACE_CREATION_FAILURE = "Failed to create a surface, Reason: {}!";
    inline std::string NO_MATERIALS = "No materials found in model!";
    inline std::string WAIT_FOR_FENCES_FAILED = "Waiting for fences failed! {}";
};

#endif
