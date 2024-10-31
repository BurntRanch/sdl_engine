#include "camera.hpp"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <chrono>
#include <engine.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <memory>
#include <new>
#include <settings.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <stdexcept>
#include <string>
#include "camera.hpp"
#include "common.hpp"
#include "model.hpp"
#include "ui/arrows.hpp"
#include "ui/label.hpp"
#include "ui/panel.hpp"
#include "util.hpp"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"
#include "tinyfiledialogs.h"

std::chrono::high_resolution_clock::time_point lastFixedFrameTime;

Camera cam(glm::vec3(1.0f, 1.0f, 1.0f));
Settings settings("settings.toml");
std::unique_ptr<Engine> engine;

enum DraggingMode {
    NOT_DRAGGING,
    DRAGGING_X_AXIS,
    DRAGGING_Y_AXIS,
    DRAGGING_Z_AXIS,
};

namespace State {
    Model **CurrentlySelectedObject = nullptr;
    UI::Arrows *CurrentlySelectedObjectArrows = nullptr;
    DraggingMode ObjectDraggingMode = NOT_DRAGGING;
    glm::vec3 ObjectPositionPreDragging = glm::vec3(0);

    std::vector<Model *> Models;

    bool IsMouseCaptured = false;

    // For some reason, it likes to spam the MMB on and off when the user holds it.
    // This check will be enabled right when it switches, and disables when it stops switching.
    bool IsMouseSpamming = false;
}

float lastX, lastY;
Uint32 lastMouseState;

void Update() {
    //fmt::println("Hi!");
}


// MEMORY LEAK!!
// Yes, I know.
void exportScene(const std::vector<Model *> &models, const std::string &path) {
    using namespace rapidxml;

    xml_document<char> sceneXML;

    xml_node<char> *node = sceneXML.allocate_node(node_type::node_element, "Scene");
    sceneXML.append_node(node);

    for (Model *model : models) {
        xml_node<char> *modelNode = sceneXML.allocate_node(node_type::node_element, "Model");
        node->append_node(modelNode);

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

    std::ofstream targetFile(path);
    targetFile << sceneXML;

    sceneXML.clear();
}

/* Import an xml scene, overwriting the current one.
    * Throws std::runtime_error and rapidxml::parse_error

Input:
    - fileName, name of the XML scene file.

Output:
    - True if the scene was sucessfully imported.
*/
bool importScene(const std::string_view fileName) {
    for (Model *model : State::Models) {
        engine->UnloadModel(model);
    }
    State::Models.clear();

    State::CurrentlySelectedObject = nullptr;

    if (State::CurrentlySelectedObjectArrows) {
        engine->RemoveUIArrows(State::CurrentlySelectedObjectArrows);
    }
    State::CurrentlySelectedObjectArrows = nullptr;

    using namespace rapidxml;

    std::ifstream sceneFile(fileName.data(), std::ios::binary | std::ios::ate);

    if (!sceneFile.good())
        return false;

    std::vector<char> sceneRawXML(static_cast<int>(sceneFile.tellg()) + 1);
    
    sceneFile.seekg(0);
    sceneFile.read(sceneRawXML.data(), sceneRawXML.size());

    xml_document<char> sceneXML;

    sceneXML.parse<0>(sceneRawXML.data());

    xml_node<char> *sceneNode = sceneXML.first_node("Scene");
    for (xml_node<char> *modelNode = sceneNode->first_node("Model"); modelNode; modelNode = modelNode->next_sibling("Model")) {
        Model *model = new Model();

        xml_node<char> *positionNode = modelNode->first_node("Position");
        NULLASSERT(positionNode);
        std::string_view positionStr(positionNode->value());
        std::vector<std::string> positionData = split(positionStr, ' ');
        model->SetPosition(glm::vec3(std::stof(positionData[0]), std::stof(positionData[1]), std::stof(positionData[2])));

        xml_node<char> *rotationNode = modelNode->first_node("Rotation");
        NULLASSERT(rotationNode);
        std::string_view rotationStr(rotationNode->value());
        std::vector<std::string> rotationData = split(rotationStr, ' ');
        model->SetRotation(glm::vec3(std::stof(rotationData[0]), std::stof(rotationData[1]), std::stof(rotationData[2])));

        xml_node<char> *scaleNode = modelNode->first_node("Scale");
        NULLASSERT(scaleNode);
        std::string_view scaleStr(scaleNode->value());
        std::vector<std::string> scaleData = split(scaleStr, ' ');
        model->SetScale(glm::vec3(std::stof(scaleData[0]), std::stof(scaleData[1]), std::stof(scaleData[2])));

        for (xml_node<char> *meshNode = modelNode->first_node("Mesh"); meshNode; meshNode = meshNode->next_sibling("Mesh")) {
            Mesh mesh;

            xml_node<char> *diffuseNode = meshNode->first_node("Diffuse");
            NULLASSERT(diffuseNode);
            std::string_view diffuseStr(diffuseNode->value());
            std::vector<std::string> diffuseData = split(diffuseStr, ' ');
            mesh.diffuse = glm::vec3(std::stof(diffuseData[0]), std::stof(diffuseData[1]), std::stof(diffuseData[2]));

            xml_node<char> *indicesNode = meshNode->first_node("Indices");
            NULLASSERT(indicesNode);
            std::string_view indicesStr(indicesNode->value());
            std::vector<std::string> indicesData = split(indicesStr, ',');
            mesh.indices.resize(indicesData.size());

            size_t i = 0;
            for (const std::string &str : indicesData) {
                mesh.indices[i] = std::stoi(str);
                i++;
            }

            xml_node<char> *diffuseMapPathNode = meshNode->first_node("DiffuseMap");
            NULLASSERT(diffuseMapPathNode);
            std::string_view diffuseMapPathStr(diffuseMapPathNode->value());
            mesh.diffuseMapPath = diffuseMapPathStr;

            for (xml_node<char> *vertexNode = meshNode->first_node("Vertex"); vertexNode; vertexNode = vertexNode->next_sibling("Vertex")) {
                Vertex vertex;

                xml_node<char> *vertexPositionNode = vertexNode->first_node("Position");
                NULLASSERT(vertexPositionNode);
                std::string_view vertexPositionStr(vertexPositionNode->value());
                std::vector<std::string> vertexPositionData = split(vertexPositionStr, ' ');
                vertex.Position = glm::vec3(std::stof(vertexPositionData[0]), std::stof(vertexPositionData[1]), std::stof(vertexPositionData[2]));

                xml_node<char> *vertexNormalNode = vertexNode->first_node("Normal");
                NULLASSERT(vertexNormalNode);
                std::string_view vertexNormalStr(vertexNormalNode->value());
                std::vector<std::string> vertexNormalData = split(vertexNormalStr, ' ');
                vertex.Normal = glm::vec3(std::stof(vertexNormalData[0]), std::stof(vertexNormalData[1]), std::stof(vertexNormalData[2]));

                xml_node<char> *vertexTexCoordNode = vertexNode->first_node("TexCoord");
                NULLASSERT(vertexTexCoordNode);
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

        engine->LoadModel(model);
        State::Models.push_back(model);
    }

    return true;
}

void checkAndSelectModel() {
    /* Sort by which model is closest to the camera, to enforce the idea that.. */
    /* When the pointer is clicking on an object that has another one behind, The player intends.. */
    /* to click the one closest to them, Because they can't see the one behind. */
    std::sort(State::Models.begin(), State::Models.end(), [] (Model *modelOne, Model *modelTwo) { return (glm::distance(cam.Position, modelOne->GetPosition()) > glm::distance(cam.Position, modelTwo->GetPosition())); });
    
    // Deselect last selected object.

    State::CurrentlySelectedObject = nullptr;

    if (State::CurrentlySelectedObjectArrows) {
        engine->RemoveUIArrows(State::CurrentlySelectedObjectArrows);
        delete State::CurrentlySelectedObjectArrows;

        State::CurrentlySelectedObjectArrows = nullptr;
    }

    for (Model *&model : State::Models) {
        if (intersects(cam.Position, cam.Front, model->GetBoundingBox())) {
            fmt::println("Left mouse button pressed on a model!");
            State::CurrentlySelectedObject = &model;

            State::CurrentlySelectedObjectArrows = new UI::Arrows(*model);

            engine->AddUIArrows(State::CurrentlySelectedObjectArrows);

            break;
        }
    }
}

void handleCamera(float x, float y) {
    if (settings.InvertHorizontal)
        x *= -1;
    if (settings.InvertVertical)
        y *= -1;

    cam.ProcessMouseMovement(x, y);
}

void FixedUpdate(const std::array<bool, 322> &keyMap) {
    float x, y;
    Uint32 mouseState;

    if (State::IsMouseCaptured) {
        mouseState = SDL_GetRelativeMouseState(&x, &y);
        State::ObjectDraggingMode = NOT_DRAGGING;
    } else {
        mouseState = SDL_GetMouseState(&x, &y);
    }

    if (!(lastMouseState & SDL_BUTTON_MMASK) && (mouseState & SDL_BUTTON_MMASK) && !State::IsMouseSpamming) {
        engine->SetMouseCaptureState(!State::IsMouseCaptured);
        State::IsMouseCaptured = !State::IsMouseCaptured;
    }
    
    if ((lastMouseState & SDL_BUTTON_MMASK) != (mouseState & SDL_BUTTON_MMASK)) {
        State::IsMouseSpamming = true;
    } else {
        State::IsMouseSpamming = false;
    }

    if (State::IsMouseCaptured) {
        handleCamera(x, y);
    } else {
        if (State::CurrentlySelectedObject && State::ObjectDraggingMode != NOT_DRAGGING) {
            Model *selectedObject = *State::CurrentlySelectedObject;
            glm::vec3 newPos;

            switch (State::ObjectDraggingMode) {
            case DRAGGING_X_AXIS:
                newPos = glm::vec3((2.0f * x / settings.RenderWidth) - 1, 0.0f, 0.0f) * cam.Right;
                newPos.x += State::ObjectPositionPreDragging.x;
                newPos.y = selectedObject->GetPosition().y;
                newPos.z = selectedObject->GetPosition().z;

                break;
            case DRAGGING_Y_AXIS:
                newPos = glm::vec3(0.0f, (2.0f * x / settings.RenderWidth) - 1, 0.0f) * cam.Right;
                newPos.x = selectedObject->GetPosition().x;
                newPos.y += State::ObjectPositionPreDragging.y;
                newPos.z = selectedObject->GetPosition().z;

                break;
            case DRAGGING_Z_AXIS:
                newPos = glm::vec3(0.0f, 0.0f, (2.0f * y / settings.RenderHeight) - 1) * -1.0f;
                newPos.x = selectedObject->GetPosition().x;
                newPos.y = selectedObject->GetPosition().y;
                newPos.z += State::ObjectPositionPreDragging.z;

                break;
            };

            selectedObject->SetPosition(newPos);
        }
        
        fmt::println("{} {}", x, y);
    }

    if (keyMap[SDL_SCANCODE_W])
        cam.ProcessKeyboard(FORWARD, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_A])
        cam.ProcessKeyboard(LEFT, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_S])
        cam.ProcessKeyboard(BACKWARD, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_D])
        cam.ProcessKeyboard(RIGHT, ENGINE_FIXED_UPDATE_DELTATIME);

    if ((mouseState & SDL_BUTTON_LMASK) && !(lastMouseState & SDL_BUTTON_LMASK)) {
        fmt::println("mouseState: {}, lastMouseState: {}, IsMouseSpamming: {}", mouseState & SDL_BUTTON_LMASK, lastMouseState & SDL_BUTTON_LMASK, State::IsMouseSpamming);
        /* TODO: Add functionality for arrows */

        float ndc_x = (2.0f * x) / settings.RenderWidth - 1.0f;
        float ndc_y = 1.0f - (2.0f * y) / settings.RenderHeight;
        float z = 1.0f;
        glm::vec3 ray_nds = glm::vec3(ndc_x, ndc_y, z);
        glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0f, 1.0f);

        glm::mat4 proj = glm::perspective(glm::radians(cam.FOV), settings.RenderWidth / (float) settings.RenderHeight, settings.CameraNear, CAMERA_FAR);

        glm::vec4 ray_eye = glm::inverse(proj) * ray_clip;
        ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

        glm::vec3 ray_world = (glm::inverse(cam.GetViewMatrix()) * ray_eye);
        // ray_world = glm::normalize(ray_world);

        // glm::vec3 frontVector = (State::IsMouseCaptured ? cam.Front : glm::vec3((2.0f - x / settings.DisplayWidth) - 1, -cam.Front.y, (2.0f - y / settings.DisplayHeight) - 1) * -1.0f);
        glm::vec3 frontVector = ray_world;

        //fmt::println("{} {} {}", frontVector.x, frontVector.y, frontVector.z);
        //fmt::println("{} {} {}", cam.Front.x, cam.Front.y, cam.Front.z);
        
        checkAndSelectModel();

        if (State::CurrentlySelectedObjectArrows && intersects(cam.Position, frontVector, State::CurrentlySelectedObjectArrows->arrowsModel->GetBoundingBox())) {
            fmt::println("Lets goo!");
            State::ObjectPositionPreDragging = (*(State::CurrentlySelectedObject))->GetPosition();

            if (intersects(cam.Position, frontVector, State::CurrentlySelectedObjectArrows->arrowsModel->meshes[0].GetBoundingBox())) {
                State::ObjectDraggingMode = DRAGGING_Z_AXIS;
                fmt::println("Green: {}", State::CurrentlySelectedObjectArrows->arrowsModel->meshes[0].diffuse.g);
            } else if (intersects(cam.Position, frontVector, State::CurrentlySelectedObjectArrows->arrowsModel->meshes[1].GetBoundingBox())) {
                State::ObjectDraggingMode = DRAGGING_Y_AXIS;
                fmt::println("Blue: {}", State::CurrentlySelectedObjectArrows->arrowsModel->meshes[1].diffuse.b);
            } else if (intersects(cam.Position, frontVector, State::CurrentlySelectedObjectArrows->arrowsModel->meshes[2].GetBoundingBox())) {
                State::ObjectDraggingMode = DRAGGING_X_AXIS;
                fmt::println("Red: {}", State::CurrentlySelectedObjectArrows->arrowsModel->meshes[2].diffuse.r);
            }
        }
    } else if (!State::IsMouseSpamming && !(mouseState & SDL_BUTTON_LMASK)) {
        State::ObjectDraggingMode = NOT_DRAGGING;
    }

    if (keyMap[SDL_SCANCODE_DELETE] && State::CurrentlySelectedObject != nullptr) {
        fmt::println("Deleting object!");

        auto selectedObjectInModel = std::find(State::Models.begin(), State::Models.end(), *State::CurrentlySelectedObject);
        if (selectedObjectInModel != State::Models.end()) {
            State::Models.erase(selectedObjectInModel);
        }

        engine->UnloadModel(*State::CurrentlySelectedObject);
        
        // make sure all other logic sees the nullptr, no dangling pointers!
        *State::CurrentlySelectedObject = nullptr;
        State::CurrentlySelectedObject = nullptr;

        if (State::CurrentlySelectedObjectArrows) {
            engine->RemoveUIArrows(State::CurrentlySelectedObjectArrows);
            delete State::CurrentlySelectedObjectArrows;

            State::CurrentlySelectedObjectArrows = nullptr;
        }
    }

    if ((keyMap[SDL_SCANCODE_LCTRL] || keyMap[SDL_SCANCODE_RCTRL]) && keyMap[SDL_SCANCODE_S]) {
        char *path = tinyfd_saveFileDialog("Select a path to export the scene.", NULL, 0, NULL, NULL);
        if (!path)
            return;

        std::string pathView(path);

        exportScene(State::Models, pathView);
    }

    if ((keyMap[SDL_SCANCODE_LCTRL] || keyMap[SDL_SCANCODE_RCTRL]) && keyMap[SDL_SCANCODE_O]) {
        std::array<const char *, 3> filterPatterns = {static_cast<const char *>("*.xml"), static_cast<const char *>("*.obj"), static_cast<const char *>("*.fbx")};
        char *path = tinyfd_openFileDialog("Select the scene to import.", NULL, filterPatterns.size(), filterPatterns.data(), NULL, false);
        if (!path)
            return;

        std::string_view pathView(path);

        if (endsWith(pathView, ".xml")) {
            try {
                importScene(pathView);
            } catch (const std::runtime_error &e) {
                tinyfd_messageBox("Scene Importer", ("The scene failed to import, Reason: " + std::string(e.what())).c_str(), "ok", "error", 1);
            }
        } else {
            Model *model = new Model(pathView);
            engine->LoadModel(model);
            State::Models.push_back(model);
        }
    }
    lastMouseState = mouseState;

    // if (keyMap[SDL_SCANCODE_UP])
    //     viking_room->SetPosition(viking_room->GetPosition() + glm::vec3(0.0f, 0.0f, 1.0f*ENGINE_FIXED_UPDATE_DELTATIME));
    // if (keyMap[SDL_SCANCODE_DOWN])
    //     viking_room->SetPosition(viking_room->GetPosition() - glm::vec3(0.0f, 0.0f, 1.0f*ENGINE_FIXED_UPDATE_DELTATIME));
    // if (keyMap[SDL_SCANCODE_RIGHT])
    //     viking_room->SetPosition(viking_room->GetPosition() + glm::vec3(1.0f*ENGINE_FIXED_UPDATE_DELTATIME, 1.0f*ENGINE_FIXED_UPDATE_DELTATIME, 0.0f) * cam.Right);
    // if (keyMap[SDL_SCANCODE_LEFT])
    //     viking_room->SetPosition(viking_room->GetPosition() - glm::vec3(1.0f*ENGINE_FIXED_UPDATE_DELTATIME, 1.0f*ENGINE_FIXED_UPDATE_DELTATIME, 0.0f) * cam.Right);
}

int main() {
    cam.MouseSensitivity = settings.MouseSensitivity;
    cam.FOV = settings.FieldOfView;
    cam.MovementSpeed = settings.Velocity;

    engine = std::make_unique<Engine>(settings, &cam);
    
    try {
        engine->Init();

        EngineSharedContext sharedContext = engine->GetSharedContext();

        engine->RegisterUpdateFunction(Update);
        engine->RegisterFixedUpdateFunction(FixedUpdate);

        std::vector<UI::GenericElement *> UIElements = UI::LoadUIFile(sharedContext, "ui.xml");
        for (UI::GenericElement *element : UIElements) {
            engine->AddUIGenericElement(element);
        }

        // UI::Arrows *arrows = new UI::Arrows(glm::vec3(0.0f, 0.0f, 0.0f));
        // engine->AddUIArrows(arrows);

        engine->Start();

        // engine->RemoveUIPanel(panel);

        // delete panel;
        
        // for (Model *&model : State::Models) {
        //     delete model;
        // }

        // if (State::CurrentlySelectedObjectWaypoint) {
        //     engine->RemoveWaypoint(State::CurrentlySelectedObjectWaypoint);
        // }
    } catch(const std::runtime_error &e) {
        fmt::println("Exception has occurred: {}", e.what());
        return -1;
    }

    return 0;
}
