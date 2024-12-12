#ifndef ERROR_HPP
#define ERROR_HPP
#include <string>
using std::string;

namespace engineError {
    inline string FAILED_SDL_INIT = "Failed to initialize SDL! (code: {})";
    inline string FAILED_WINDOW_INIT = "Failed to create window! (error: {})";
    inline string FAILED_VULKAN_LOAD = "Failed to load vulkan!";
    inline string FAILED_VULKAN_EXTS = "Failed to get extensions for vulkan!";
    inline string NO_VULKAN_DEVICES = "Failed to get vulkan capable devices!";
    inline string NO_QUEUE_FAMILIES = "Failed to get queue families!";
    inline string CANT_CREATE_DEVICE = "Failed to create logical device!";
    inline string NO_CAPABLE_CARD = "Failed to find a capable card!";
    inline string SWAPCHAIN_INIT_FAILURE = "Failed to create a swapchain! (Error: {})";
    inline string IMAGE_VIEW_CREATION_FAILURE = "Failed to create an image view!";
    inline string RENDERPASS_CREATION_FAILURE = "Failed to create a render pass!";
    inline string PIPELINE_CREATION_FAILURE = "Failed to create a graphics pipeline!";
    inline string PIPELINE_LAYOUT_CREATION_FAILURE = "Failed to create a graphics pipeline layout!";
    inline string FRAMEBUFFER_CREATION_FAILURE = "Failed to create framebuffers!";
    inline string COMMAND_POOL_CREATION_FAILURE = "Failed to create command pool!";
    inline string COMMAND_BUFFER_ALLOCATION_FAILURE = "Failed to allocate command buffer!";
    inline string COMMAND_BUFFER_BEGIN_FAILURE = "Failed to begin recording the command buffer!";
    inline string COMMAND_BUFFER_END_FAILURE = "Failed to end recording of the command buffer!";
    inline string SYNC_OBJECTS_CREATION_FAILURE = "Failed to create sync objects!";
    inline string QUEUE_SUBMIT_FAILURE = "Failed to submit command buffer to queue!";
    inline string INSTANCE_CREATION_FAILURE = "Failed to create vulkan instance!";
    inline string CANT_ACQUIRE_NEXT_IMAGE = "Failed to acquire next swapchain image!";
    inline string CANT_CREATE_VERTEX_BUFFER = "Failed to create a vertex buffer!";
    inline string CANT_FIND_SUITABLE_MEMTYPE = "Failed to find a suitable memory type!";
    inline string CANT_ALLOCATE_MEMORY = "Failed to allocate device memory!";
    inline string VERTEX_BUFFER_ALREADY_EXISTS = "Tried to create a vertex buffer more than twice!";
    inline string INDEX_BUFFER_ALREADY_EXISTS = "Tried to create an index buffer more than twice!";
    inline string DESCRIPTOR_SET_LAYOUT_CREATION_FAILURE = "Failed to create descriptor set layout!";
    inline string TEXTURE_LOADING_FAILURE = "Failed to load texture file, Reason: {}, File: ({})!";
    inline string IMAGE_CREATION_FAILURE = "Failed to create image!";
    inline string UNSUPPORTED_LAYOUT_TRANSITION = "Unsupported image layout transition!";
    inline string SAMPLER_CREATION_FAILURE = "Failed to create sampler!";
    inline string INVALID_CHANNEL_COUNT = "Attempted to convert invalid channel count to VkFormat!";
    inline string UNSUPPORTED_FORMAT = "Attempted to convert unsupported VkFormat to channel count!";
    inline string CANT_FIND_ANY_FORMAT = "Tried to find best format, but none can be used!";
    inline string RENDERPASS_PIPELINE_EXISTS = "Tried to create a graphics pipeline for a renderpass that already has a graphics pipeline!";
    inline string SURFACE_CREATION_FAILURE = "Failed to create a surface, Reason: {}!";
    inline string NO_MATERIALS = "No materials found in model!";
};

#endif