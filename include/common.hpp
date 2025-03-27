#ifndef COMMON_HPP
#define COMMON_HPP


#include "settings.hpp"

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float2.hpp>
#include <mutex>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL_stdinc.h>

class BaseRenderer;

struct BufferAndMemory {
    VkBuffer buffer;
    VkDeviceMemory memory;
    Uint32 size;
    void *mappedData = nullptr;
};

struct ImageAndMemory {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;

    Uint32 size;
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
    VkFormat format;
};

struct EngineSharedContext {
    BaseRenderer *renderer;

    VkDevice engineDevice;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue graphicsQueue;
    Settings &settings;

    std::mutex &singleTimeCommandMutex;
};

struct GlyphUBO {
    glm::vec2 Offset;
};

struct Glyph {
    glm::vec2 offset;   // offset from the start of the string, from -1.0f to 1.0f
    glm::vec2 scale;
    char character;
    std::string fontIdentifier;   // Identifies the font by family name, style name, and height.
    std::optional<std::pair<TextureImageAndMemory, BufferAndMemory>> glyphBuffer;  // If it's a space or a newline, there won't be any glyph.

    GlyphUBO glyphUBO;
    BufferAndMemory glyphUBOBuffer;
};


namespace UI {
    enum ElementType {
        UNKNOWN,
        SCALABLE,   // Has the Scale property.
        PANEL,
        LABEL,
        BUTTON,
        ARROWS,
        WAYPOINT
    };

    enum FitType {
        UNSET,  // If the scalable has a parent, check the parents fit type.
        NONE,
        FIT_CHILDREN,    // Scales the Scalable object to fit its children.
    };

    class GenericElement {
    public:
        std::string id;

        ElementType genericType;
        ElementType type;

        virtual void SetPosition(glm::vec2 position);
        virtual glm::vec2 GetPosition();

        virtual void SetDepth(float depth);
        virtual float GetDepth();

        virtual void SetVisible(bool visible);
        virtual bool GetVisible();

        virtual void SetParent(GenericElement *parent);
        virtual GenericElement *GetParent();

        virtual void AddChild(GenericElement *element);
        virtual void RemoveChild(GenericElement *child);
        virtual std::vector<GenericElement *> GetChildren();

        virtual void DestroyBuffers();
    protected:
        glm::vec2 m_Position;

        bool m_Visible = true;

        GenericElement *m_Parent = nullptr;
        std::vector<GenericElement *> m_Children;

        float m_Depth;
    };

    class Scalable : public GenericElement {
    public:
        FitType fitType = UNSET;

        Scalable();

        virtual void SetScale(glm::vec2 scales);

        virtual glm::vec2 GetScale();

        /* Get the Scale without applying fitType effects. */
        virtual glm::vec2 GetUnfitScale();
    protected:
        glm::vec2 m_Scale;
    };
}

#endif // !COMMON_HPP
