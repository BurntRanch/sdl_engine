#ifndef ERROR_HPP
#define ERROR_HPP
#include <string_view>

namespace engineError {
    inline constexpr std::string_view FAILED_SDL_INIT = "Failed to initialize SDL! (code: {})";
    inline constexpr std::string_view FAILED_WINDOW_INIT = "Failed to create window! (error: {})";
    inline constexpr std::string_view FAILED_VULKAN_LOAD = "Failed to load vulkan!";
    inline constexpr std::string_view FAILED_VULKAN_EXTS = "Failed to get extensions for vulkan!";
    inline constexpr std::string_view NO_VULKAN_DEVICES = "Failed to get vulkan capable devices!";
    inline constexpr std::string_view NO_QUEUE_FAMILIES = "Failed to get queue families!";
    inline constexpr std::string_view CANT_CREATE_DEVICE = "Failed to create logical device!";
    inline constexpr std::string_view NO_CAPABLE_CARD = "Failed to find a capable card!";
    inline constexpr std::string_view SWAPCHAIN_INIT_FAILURE = "Failed to create a swapchain! (Error: {})";
    inline constexpr std::string_view IMAGE_VIEW_CREATION_FAILURE = "Failed to create an image view!";
    inline constexpr std::string_view RENDERPASS_CREATION_FAILURE = "Failed to create a render pass!";
    inline constexpr std::string_view PIPELINE_CREATION_FAILURE = "Failed to create a graphics pipeline!";
    inline constexpr std::string_view PIPELINE_LAYOUT_CREATION_FAILURE = "Failed to create a graphics pipeline layout!";
    inline constexpr std::string_view FRAMEBUFFER_CREATION_FAILURE = "Failed to create framebuffers!";
    inline constexpr std::string_view COMMAND_POOL_CREATION_FAILURE = "Failed to create command pool!";
    inline constexpr std::string_view COMMAND_BUFFER_ALLOCATION_FAILURE = "Failed to allocate command buffer!";
    inline constexpr std::string_view COMMAND_BUFFER_BEGIN_FAILURE = "Failed to begin recording the command buffer!";
    inline constexpr std::string_view COMMAND_BUFFER_END_FAILURE = "Failed to end recording of the command buffer!";
    inline constexpr std::string_view SYNC_OBJECTS_CREATION_FAILURE = "Failed to create sync objects!";
    inline constexpr std::string_view QUEUE_SUBMIT_FAILURE = "Failed to submit command buffer to queue! {}";
    inline constexpr std::string_view INSTANCE_CREATION_FAILURE = "Failed to create vulkan instance!";
    inline constexpr std::string_view CANT_ACQUIRE_NEXT_IMAGE = "Failed to acquire next swapchain image!";
    inline constexpr std::string_view CANT_CREATE_VERTEX_BUFFER = "Failed to create a vertex buffer!";
    inline constexpr std::string_view CANT_FIND_SUITABLE_MEMTYPE = "Failed to find a suitable memory type!";
    inline constexpr std::string_view CANT_ALLOCATE_MEMORY = "Failed to allocate device memory!";
    inline constexpr std::string_view VERTEX_BUFFER_ALREADY_EXISTS = "Tried to create a vertex buffer more than twice!";
    inline constexpr std::string_view INDEX_BUFFER_ALREADY_EXISTS = "Tried to create an index buffer more than twice!";
    inline constexpr std::string_view DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE = "Failed to create descriptor set layout!";
    inline constexpr std::string_view TEXTURE_LOADING_FAILURE = "Failed to load texture file, Reason: {}, File: ({})!";
    inline constexpr std::string_view IMAGE_CREATION_FAILURE = "Failed to create image!";
    inline constexpr std::string_view UNSUPPORTED_LAYOUT_TRANSITION = "Unsupported image layout transition!";
    inline constexpr std::string_view SAMPLER_CREATION_FAILURE = "Failed to create sampler!";
    inline constexpr std::string_view INVALID_CHANNEL_COUNT = "Attempted to convert invalid channel count to VkFormat!";
    inline constexpr std::string_view UNSUPPORTED_FORMAT = "Attempted to convert unsupported VkFormat to channel count!";
    inline constexpr std::string_view CANT_FIND_ANY_FORMAT = "Tried to find best format, but none can be used!";
    inline constexpr std::string_view RENDERPASS_PIPELINE_EXISTS = "Tried to create a graphics pipeline for a renderpass that already has a graphics pipeline!";
    inline constexpr std::string_view SURFACE_CREATION_FAILURE = "Failed to create a surface, Reason: {}!";
    inline constexpr std::string_view NO_MATERIALS = "No materials found in model!";
    inline constexpr std::string_view WAIT_FOR_FENCES_FAILED = "Waiting for fences failed! {}";
};

#endif
