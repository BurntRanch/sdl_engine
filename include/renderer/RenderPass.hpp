#pragma once

#include "renderer/GraphicsPipeline.hpp"
#include "renderer/baseRenderer.hpp"
#include <SDL3/SDL_stdinc.h>
#include <any>
#include <glm/ext/vector_float4.hpp>

/* RenderPasses contain multiple graphics pipelines, and each graphics pipeline contains multiple Shaders. */
class RenderPass {
public:
    /* RawRenderPass should be used by the Renderer and not by us, thus we don't care about its type. */
    RenderPass(BaseRenderer *renderer, std::any rawRenderPass, glm::vec2 resolution);

    void SetClearColor(glm::vec4 clearColor);
    glm::vec4 GetClearColor();

    void SetSubpass(Uint32 index, GraphicsPipeline *pipeline);

    void SetRawRenderPass(std::any rawRenderPass);
    void SetRenderer(BaseRenderer *renderer);
    void SetResolution(glm::vec2 resolution);

    std::any GetRawRenderPass();
    BaseRenderer *GetRenderer();
    glm::vec2 GetResolution();

    void Execute(std::any rawFramebuffer);
private:
    BaseRenderer *m_Renderer;
    std::any m_RawRenderPass;

    glm::vec2 m_Resolution;

    std::vector<std::optional<GraphicsPipeline *>> m_Subpasses;

    glm::vec4 m_ClearColor;
};