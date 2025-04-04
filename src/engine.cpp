#include "engine.hpp"

#include "BulletCollision/BroadphaseCollision/btDbvtBroadphase.h"
#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h"
#include "BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h"
#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btVector3.h"
#include "Node/Node3D/Light3D/PointLight3D/PointLight3D.hpp"
#include "SceneTree.hpp"

#include "common.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include "isteamnetworkingsockets.h"
#include "networking/connection.hpp"
#include "Node/Node.hpp"
#include "Node/Node3D/Model3D/Model3D.hpp"
#include "Node/Node3D/Camera3D/Camera3D.hpp"
#include "renderer/GraphicsPipeline.hpp"
#include "renderer/Shader.hpp"
#include "renderer/baseRenderer.hpp"
#include "renderer/vulkanRenderer.hpp"
#include "steamclientpublic.h"
#include "steamnetworkingsockets.h"
#include "steamnetworkingtypes.h"
#include "ui/button.hpp"
#include "ui/panel.hpp"
#include "ui.hpp"
#include "util.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <glm/fwd.hpp>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>

Engine::Engine() {
    SDL_Init(SDL_INIT_EVENTS);

    m_SceneTree = new SceneTree();
    m_SceneTree->RegisterLoadListener(std::bind(&Engine::LoadNode, this, std::placeholders::_1));
    m_SceneTree->RegisterUnloadListener(std::bind(&Engine::UnloadNode, this, std::placeholders::_1));
}

Engine::~Engine() {
    // DeinitPhysics();
}

/* Basic Shaders are those with only vertex/fragment shaders */
GraphicsPipeline *CreateBasicShader(BaseRenderer *renderer, std::string name, RenderPass *renderPass, Uint32 subpassIndex, VkFrontFace frontFace, glm::vec4 viewport, glm::vec4 scissor, const DescriptorLayout &descriptorSetLayout, bool isSimple = VK_FALSE, bool enableDepth = VK_TRUE) {
    std::vector<Shader> shaders;

    shaders.emplace_back(renderer, VK_SHADER_STAGE_VERTEX_BIT, std::filesystem::path("shaders") / (name + ".vert.spv"));
    shaders.emplace_back(renderer, VK_SHADER_STAGE_FRAGMENT_BIT, std::filesystem::path("shaders") / (name + ".frag.spv"));

    return renderer->CreateGraphicsPipeline(shaders, renderPass, subpassIndex, frontFace, viewport, scissor, descriptorSetLayout, isSimple, enableDepth);
}

void Engine::InitRenderer(Settings &settings) {
    m_Settings = &settings;

    m_Renderer = new VulkanRenderer(settings);

    m_Renderer->Init();

    // Render
    glm::vec4 renderViewport, renderScissor;

    renderViewport.x = 0.0f;
    renderViewport.y = 0.0f;
    renderViewport.z = (float) m_Settings->RenderWidth;
    renderViewport.w = (float) m_Settings->RenderHeight;

    renderScissor.x = 0;
    renderScissor.y = 0;
    renderScissor.z = m_Settings->RenderWidth;
    renderScissor.w = m_Settings->RenderHeight;

    // Rescale
    glm::vec4 displayViewport, displayScissor;

    displayViewport.x = 0.0f;
    displayViewport.y = 0.0f;
    displayViewport.z = (float) m_Settings->DisplayWidth;
    displayViewport.w = (float) m_Settings->DisplayHeight;

    displayScissor.x = 0;
    displayScissor.y = 0;
    displayScissor.z = m_Settings->DisplayWidth;
    displayScissor.w = m_Settings->DisplayHeight;

    DescriptorLayout renderLayout(m_Renderer);

    /* Matrices UBO */
    renderLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0});
    /* Materials UBO */
    renderLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1});
    /* Lights UBO */
    renderLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2});

    renderLayout.Create();


    DescriptorLayout waypointLayout(m_Renderer);

    /* Matrices UBO */
    waypointLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0});
    /* waypoint UBO */
    waypointLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1});

    waypointLayout.Create();


    DescriptorLayout rescaleLayout(m_Renderer);

    /* Render image */
    rescaleLayout.AddBinding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0});

    rescaleLayout.Create();


    DescriptorLayout panelLayout(m_Renderer);

    /* Dimensions UBO */
    panelLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0});
    /* Color/Texture */
    panelLayout.AddBinding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1});

    panelLayout.Create();


    DescriptorLayout labelLayout(m_Renderer);

    /* Label info */
    labelLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0});
    /* Glyph texture */
    labelLayout.AddBinding({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1});
    /* Glyph info */
    labelLayout.AddBinding({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2});

    labelLayout.Create();

    std::vector<Shader> lightingShaders;
    lightingShaders.emplace_back(m_Renderer, VK_SHADER_STAGE_VERTEX_BIT, 
        std::filesystem::path("shaders") / std::filesystem::path("lighting") / ("lighting.vert.spv")
    );
    lightingShaders.emplace_back(m_Renderer, VK_SHADER_STAGE_FRAGMENT_BIT, 
        std::filesystem::path("shaders") / std::filesystem::path("lighting") / ("untextured_lighting.frag.spv")
    );

    GraphicsPipeline *m_MainGraphicsPipeline = m_Renderer->CreateGraphicsPipeline(
        lightingShaders, m_Renderer->m_MainRenderPass, 
        0, VK_FRONT_FACE_CLOCKWISE, 
        renderViewport, renderScissor,
        {renderLayout}
    );
    m_MainGraphicsPipeline->SetRenderFunction(std::bind(&Engine::MainRenderFunction, this, std::placeholders::_1));
    
    GraphicsPipeline *m_UIWaypointGraphicsPipeline = CreateBasicShader(m_Renderer,
        "uiwaypoint", m_Renderer->m_MainRenderPass, 
        1, VK_FRONT_FACE_CLOCKWISE, 
        renderViewport, renderScissor, 
        {waypointLayout}, 
        true, false);
    m_UIWaypointGraphicsPipeline->SetRenderFunction(std::bind(&Engine::UIWaypointRenderFunction, this, std::placeholders::_1));
    
    GraphicsPipeline *m_RescaleGraphicsPipeline = CreateBasicShader(m_Renderer,
        "rescale", m_Renderer->m_RescaleRenderPass, 
        0, VK_FRONT_FACE_CLOCKWISE, 
        displayViewport, displayScissor,
        {rescaleLayout},
        true);
    m_RescaleGraphicsPipeline->SetRenderFunction(std::bind(&Engine::RescaleRenderFunction, this, std::placeholders::_1));
    
    GraphicsPipeline *m_UIPanelGraphicsPipeline = CreateBasicShader(m_Renderer,
        "uipanel", m_Renderer->m_RescaleRenderPass,
        1, VK_FRONT_FACE_CLOCKWISE, 
        displayViewport, displayScissor,
        {panelLayout}, 
        true);
    m_UIPanelGraphicsPipeline->SetRenderFunction(std::bind(&Engine::UIPanelRenderFunction, this, std::placeholders::_1));
    
    GraphicsPipeline *m_UILabelGraphicsPipeline = CreateBasicShader(m_Renderer, 
        "uilabel", m_Renderer->m_RescaleRenderPass, 
        2, VK_FRONT_FACE_CLOCKWISE, 
        displayViewport, displayScissor, 
        {labelLayout}, 
        true);
    m_UILabelGraphicsPipeline->SetRenderFunction(std::bind(&Engine::UILabelRenderFunction, this, std::placeholders::_1));

    RegisterSDLEventListener(std::bind(&Engine::CheckButtonClicks, this, std::placeholders::_1), SDL_EVENT_MOUSE_BUTTON_UP);
}

// void Engine::InitPhysics() {
//     m_CollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
//     m_Dispatcher = std::make_unique<btCollisionDispatcher>(m_CollisionConfig.get());
//     m_Broadphase = std::make_unique<btDbvtBroadphase>();
//     m_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();

//     m_DynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(m_Dispatcher.get(), m_Broadphase.get(), m_Solver.get(), m_CollisionConfig.get());

//     m_DynamicsWorld->setGravity(btVector3(0, -5, 0));

//     for (auto &rigidBodyPtr : m_RigidBodies) {
//         m_DynamicsWorld->addRigidBody(rigidBodyPtr.get());
//     }

//     RegisterFixedUpdateFunction(std::bind(&Engine::PhysicsStep, this));
// }

// void Engine::DeinitPhysics() {
//     if (m_DynamicsWorld) {
//         while (m_DynamicsWorld->getNumConstraints() > 0) {
//             m_DynamicsWorld->removeConstraint(m_DynamicsWorld->getConstraint(0));
//         }

//         btCollisionObjectArray &array = m_DynamicsWorld->getCollisionObjectArray();

//         while (array.size() > 0) {
//             btCollisionObject *obj = array[0];
//             btRigidBody *rigidBody = btRigidBody::upcast(obj);

//             if (rigidBody && rigidBody->getMotionState()) {
//                 delete rigidBody->getMotionState();
//             }

//             if (obj->getCollisionShape()) {
//                 delete obj->getCollisionShape();
//             }

//             m_DynamicsWorld->removeCollisionObject(obj);
//             delete obj;
//         }
//     }

//     m_DynamicsWorld.reset();
//     m_Solver.reset();
//     m_Broadphase.reset();
//     m_Dispatcher.reset();
//     m_CollisionConfig.reset();
// }

void Engine::RegisterUIButtonListener(const std::function<void(std::string)>& listener) {
    m_UIButtonListeners.push_back(listener);
}

void Engine::LoadNode(const Node *node) {
    if (m_Renderer && typeid(*node) == typeid(Model3D)) {
        m_Renderer->LoadModel(reinterpret_cast<const Model3D *>(node));
    }

    // if (object->GetRigidBody()) {
    //     auto &rigidBodyPtr = object->GetRigidBody();

    //     m_RigidBodies.push_back(rigidBodyPtr);

    //     if (m_DynamicsWorld) {
    //         m_DynamicsWorld->addRigidBody(rigidBodyPtr.get());
    //     }
    // }
}

void Engine::UnloadNode(const Node *node) {
    if (m_Renderer && typeid(*node) == typeid(Model3D)) {
        m_Renderer->UnloadModel(reinterpret_cast<const Model3D *>(node));
    }
}

void Engine::MainRenderFunction(GraphicsPipeline *pipeline) {
    glm::mat4 viewMatrix, projectionMatrix;
    Camera3D *mainCamera3D = m_SceneTree->GetMainCamera3D();

    if (mainCamera3D) {
        viewMatrix = mainCamera3D->GetViewMatrix();

        /* TODO: How do we logically choose between the Settings FOV and the camera FOV? */
        projectionMatrix = glm::perspective(glm::radians(mainCamera3D->GetFOV()), (float)m_Settings->RenderWidth / (float)m_Settings->RenderHeight, mainCamera3D->GetNear(), mainCamera3D->GetFar());

        // invert Y axis, glm was meant for OpenGL which inverts the Y axis.
        projectionMatrix[1][1] *= -1;

        LightsUBO lightsUBO;

        for (const PointLight3D *pointLight : m_SceneTree->GetPointLight3Ds()) {
            lightsUBO.pointLights[lightsUBO.pointLightCount].color = glm::vec4(pointLight->GetLightColor(), 1);
            lightsUBO.pointLights[lightsUBO.pointLightCount++].attenuation = glm::vec4(pointLight->GetAttenuation(), 1);
        }

        SDL_memcpy(m_Renderer->m_LightsUBOBuffer.mappedData, &lightsUBO, sizeof(lightsUBO));

        for (RenderMesh &renderModel : m_Renderer->m_RenderModels) {
            renderModel.matricesUBO.modelMatrix = renderModel.model->GetModelMatrix();

            renderModel.matricesUBO.viewMatrix = viewMatrix;
            renderModel.matricesUBO.projectionMatrix = projectionMatrix;

            SDL_memcpy(renderModel.matricesUBOBuffer.mappedData, &renderModel.matricesUBO, sizeof(renderModel.matricesUBO));

            renderModel.materialUBO.colors = renderModel.mesh->GetMaterial().GetColor();

            SDL_memcpy(renderModel.materialsUBOBuffer.mappedData, &renderModel.materialUBO, sizeof(renderModel.materialUBO));

            pipeline->UpdateBindingValue(0, renderModel.matricesUBOBuffer);
            pipeline->UpdateBindingValue(1, renderModel.materialsUBOBuffer);
            pipeline->UpdateBindingValue(2, m_Renderer->m_LightsUBOBuffer);

            m_Renderer->Draw(pipeline, renderModel.vertexBuffer, 0, renderModel.indexBuffer, renderModel.indexBufferSize);
        }
    }
}
void Engine::UIWaypointRenderFunction(GraphicsPipeline *pipeline) {
    glm::mat4 viewMatrix, projectionMatrix;
    Camera3D *mainCamera3D = m_SceneTree->GetMainCamera3D();

    if (mainCamera3D) {
        viewMatrix = mainCamera3D->GetViewMatrix();

        projectionMatrix = glm::perspective(glm::radians(mainCamera3D->GetFOV()), (float)m_Settings->RenderWidth / (float)m_Settings->RenderHeight, mainCamera3D->GetNear(), mainCamera3D->GetFar());

        for (RenderUIWaypoint &renderUIWaypoint : m_Renderer->m_RenderUIWaypoints) {
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

            m_Renderer->Draw(pipeline, m_Renderer->m_FullscreenQuadVertexBuffer, 6);
        }
    }
}
void Engine::RescaleRenderFunction(GraphicsPipeline *pipeline) {
    pipeline->UpdateBindingValue(0, m_Renderer->m_RenderImageAndMemory);

    m_Renderer->Draw(pipeline, m_Renderer->m_FullscreenQuadVertexBuffer, 6);
}
void Engine::UIPanelRenderFunction(GraphicsPipeline *pipeline) {
    for (RenderUIPanel &renderUIPanel : m_Renderer->m_UIPanels) {
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

        m_Renderer->Draw(pipeline, m_Renderer->m_FullscreenQuadVertexBuffer, 6);
    }
}
void Engine::UILabelRenderFunction(GraphicsPipeline *pipeline) {
    for (RenderUILabel &renderUILabel : m_Renderer->m_UILabels) {
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

            m_Renderer->Draw(pipeline, glyph.glyphBuffer.value().second, 6);
        }
    }
}

BaseRenderer *Engine::GetRenderer() {
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

void Engine::Start() {
    using namespace std::chrono;

    high_resolution_clock::time_point lastLoopStartTime = high_resolution_clock::now(), loopStartTime = high_resolution_clock::now();

    bool shouldQuit = false;
    SDL_Event event;

    /* Each loop turn will add its delta time to this variable, and this will be used to execute certain tasks only every x seconds. */
    double accumulative = 0.0;

    while (!shouldQuit) {
        loopStartTime = high_resolution_clock::now();
        accumulative += duration_cast<duration<double>>(loopStartTime - lastLoopStartTime).count();

        while (SDL_PollEvent(&event)) {
            if (QuitEventCheck(event))
                shouldQuit = true;

            try {
                const auto &listeners = m_SDLEventToListenerMap.at(static_cast<SDL_EventType>(event.type));

                for (const auto &listener : listeners) {
                    listener(&event);
                }
            } catch (const std::out_of_range &e) {
                continue;
            }
        }

        for (const auto &func : m_UpdateFunctions) {
            func();
        }

        /* Fixed updates, every 60th of a second. */
        if (accumulative >= ENGINE_FIXED_UPDATE_DELTATIME) {
            accumulative -= ENGINE_FIXED_UPDATE_DELTATIME;

            for (const auto &fixedUpdateFunc : m_FixedUpdateFunctions) {
                fixedUpdateFunc();
            }
        }

        if (m_Renderer) {
            m_Renderer->StepRender();
        }

        lastLoopStartTime = loopStartTime;
    }
}

void Engine::LoadUIFile(const std::string &name) {
    if (m_Renderer == nullptr) {
        return;
    }

    const std::vector<UI::GenericElement *>& UIElements = UI::LoadUIFile(m_Renderer, name);
    for (UI::GenericElement *element : UIElements) {
        m_Renderer->AddUIGenericElement(element);

        if (element->type == UI::BUTTON) {
            RegisterUIButton(reinterpret_cast<UI::Button *>(element));
        }

        m_UIElements.push_back(element);

        std::vector<std::vector<UI::GenericElement *>> UIElementChildren {element->GetChildren()};

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

SceneTree *Engine::GetSceneTree() {
    return m_SceneTree;
}

void Engine::ImportScene(const std::string &path) {
    m_SceneTree->ImportFromGLTF2(path);
}

/* TODO: Implement with assimp */
// void Engine::ExportScene(const std::string &path) {
//     using namespace rapidxml;

//     xml_document<char> sceneXML;

//     xml_node<char> *node = sceneXML.allocate_node(node_type::node_element, "Scene");
//     sceneXML.append_node(node);

//     for (Object *object : m_Objects) {
//         xml_node<char> *objectNode = sceneXML.allocate_node(node_type::node_element, "Object");
//         node->append_node(objectNode);

//         xml_node<char> *objectIDNode = sceneXML.allocate_node(node_type::node_element, "ObjectID", fmt::to_string(object->GetObjectID()).c_str());
//         objectNode->append_node(objectIDNode);
        
//         glm::vec3 position = object->GetPosition();
//         glm::vec3 rotation = object->GetRotation();
//         glm::vec3 scale = object->GetScale();

//         std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
//         xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
//         objectNode->append_node(positionNode);

//         std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
//         xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
//         objectNode->append_node(rotationNode);

//         std::string scaleStr = fmt::format("{} {} {}", scale.x, scale.y, scale.z);
//         xml_node<char> *scaleNode = sceneXML.allocate_node(node_type::node_element, "Scale", sceneXML.allocate_string(scaleStr.c_str()));
//         objectNode->append_node(scaleNode);

//         for (Model *model : object->GetModelAttachments()) {
//             xml_node<char> *modelNode = sceneXML.allocate_node(node_type::node_element, "Model");
//             objectNode->append_node(modelNode);

//             glm::vec3 position = model->GetPosition();
//             glm::vec3 rotation = model->GetRotation();
//             glm::vec3 scale = model->GetScale();

//             std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
//             xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
//             modelNode->append_node(positionNode);

//             std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
//             xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
//             modelNode->append_node(rotationNode);

//             std::string scaleStr = fmt::format("{} {} {}", scale.x, scale.y, scale.z);
//             xml_node<char> *scaleNode = sceneXML.allocate_node(node_type::node_element, "Scale", sceneXML.allocate_string(scaleStr.c_str()));
//             modelNode->append_node(scaleNode);

//             for (Mesh &mesh : model->meshes) {
//                 xml_node<char> *meshNode = sceneXML.allocate_node(node_type::node_element, "Mesh");
//                 modelNode->append_node(meshNode);

//                 std::string diffuseStr = fmt::format("{} {} {}", mesh.diffuse.x, mesh.diffuse.y, mesh.diffuse.z);
//                 xml_node<char> *diffuseNode = sceneXML.allocate_node(node_type::node_element, "Diffuse", sceneXML.allocate_string(diffuseStr.c_str()));
//                 meshNode->append_node(diffuseNode);

//                 std::string indicesStr = fmt::to_string(fmt::join(mesh.indices, ","));
//                 xml_node<char> *indicesNode = sceneXML.allocate_node(node_type::node_element, "Indices", sceneXML.allocate_string(indicesStr.c_str()));
//                 meshNode->append_node(indicesNode);

//                 xml_node<char> *diffuseMapPathNode = sceneXML.allocate_node(node_type::node_element, "DiffuseMap", mesh.diffuseMapPath.c_str());
//                 meshNode->append_node(diffuseMapPathNode);

//                 for (Vertex vert : mesh.vertices) {
//                     xml_node<char> *vertexNode = sceneXML.allocate_node(node_type::node_element, "Vertex");
//                     meshNode->append_node(vertexNode);

//                     std::string vertPositionStr = fmt::format("{} {} {}", vert.Position.x, vert.Position.y, vert.Position.z);
//                     xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(vertPositionStr.c_str()));
//                     vertexNode->append_node(positionNode);
                    
//                     std::string vertNormalStr = fmt::format("{} {} {}", vert.Normal.x, vert.Normal.y, vert.Normal.z);
//                     xml_node<char> *normalNode = sceneXML.allocate_node(node_type::node_element, "Normal", sceneXML.allocate_string(vertNormalStr.c_str()));
//                     vertexNode->append_node(normalNode);
                    
//                     std::string vertTexCoordStr = fmt::format("{} {}", vert.TexCoord.x, vert.TexCoord.y);
//                     xml_node<char> *texCoordNode = sceneXML.allocate_node(node_type::node_element, "TexCoord", sceneXML.allocate_string(vertTexCoordStr.c_str()));
//                     vertexNode->append_node(texCoordNode);
//                 }
//             }
//         }
//     }

//     std::ofstream targetFile(path);
//     targetFile << sceneXML;

//     sceneXML.clear();
// }

UI::GenericElement *Engine::GetElementByID(const std::string &id) {
    for (UI::GenericElement *&element : m_UIElements) {
        if (element->id == id) {
            return element;
        }
    }

    return nullptr;
}

void Engine::RegisterUpdateFunction(const std::function<void()> &func) {
    m_UpdateFunctions.push_back(func);
}

void Engine::RegisterFixedUpdateFunction(const std::function<void()> &func) {
    m_FixedUpdateFunctions.push_back(func);
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

void Engine::RegisterSDLEventListener(const std::function<void(SDL_Event *)> &func, SDL_EventType types) {
    if (m_SDLEventToListenerMap.find(types) == m_SDLEventToListenerMap.end()) {
        m_SDLEventToListenerMap.insert(std::make_pair(types, std::vector<std::function<void(SDL_Event *)>>()));
    }
    
    m_SDLEventToListenerMap[types].push_back(func);
}

void Engine::PhysicsStep() {
    if (!m_DynamicsWorld) {
        return;
    }

    m_DynamicsWorld->stepSimulation(ENGINE_FIXED_UPDATE_DELTATIME, 4, ENGINE_FIXED_UPDATE_DELTATIME / 4.0f);

    /* TODO: in the far future, we might be able to do softbodies! */
    for (int i = m_DynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--) {
        btCollisionObject *collisionObject = m_DynamicsWorld->getCollisionObjectArray()[i];
        btRigidBody *body = btRigidBody::upcast(collisionObject);

        btTransform transform;

        if (body && body->getMotionState()) {
            body->getMotionState()->getWorldTransform(transform);
        } else {
            transform = collisionObject->getWorldTransform();
        }

        btVector3 origin = transform.getOrigin();
        btQuaternion rotation = transform.getRotation();

        /* TODO: add PhysicsBody3D or smth */
        Node3D *obj = reinterpret_cast<Node3D *>(body->getUserPointer());
        UTILASSERT(obj);

        obj->SetPosition(glm::vec3(origin.getX(), origin.getY(), origin.getZ()));
        obj->SetRotation(glm::quat(rotation.getW(), rotation.getX(), rotation.getY(), rotation.getZ()));
    }
}
