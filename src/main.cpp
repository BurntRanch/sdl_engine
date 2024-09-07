#include "camera.hpp"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <chrono>
#include <engine.hpp>
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
#include "ui/panel.hpp"
#include "util.hpp"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"
#include "tinyfiledialogs.h"

std::chrono::high_resolution_clock::time_point lastFixedFrameTime;

Camera cam(glm::vec3(1.0f, 1.0f, 1.0f));
Settings settings("settings.toml");
std::unique_ptr<Engine> engine;

namespace State {
    Model **CurrentlySelectedObject = nullptr;
    Particle *CurrentlySelectedObjectParticle = nullptr;

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

        std::string positionStr = fmt::format("{} {} {}", position.x, position.y, position.z);
        xml_node<char> *positionNode = sceneXML.allocate_node(node_type::node_element, "Position", sceneXML.allocate_string(positionStr.c_str()));
        modelNode->append_node(positionNode);

        std::string rotationStr = fmt::format("{} {} {}", rotation.x, rotation.y, rotation.z);
        xml_node<char> *rotationNode = sceneXML.allocate_node(node_type::node_element, "Rotation", sceneXML.allocate_string(rotationStr.c_str()));
        modelNode->append_node(rotationNode);

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
bool importScene(const std::string &fileName) {
    for (Model *model : State::Models) {
        engine->UnloadModel(model);
    }
    State::Models.clear();

    State::CurrentlySelectedObject = nullptr;

    if (State::CurrentlySelectedObjectParticle) {
        engine->RemoveParticle(State::CurrentlySelectedObjectParticle);
    }
    State::CurrentlySelectedObjectParticle = nullptr;

    using namespace rapidxml;

    std::ifstream sceneFile(fileName, std::ios::binary | std::ios::ate);

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

                model->boundingBox[0].x = std::max(model->boundingBox[0].x, vertex.Position.x);
                model->boundingBox[0].y = std::max(model->boundingBox[0].y, vertex.Position.y);
                model->boundingBox[0].z = std::max(model->boundingBox[0].z, vertex.Position.z);
                model->boundingBox[1].x = std::min(model->boundingBox[1].x, vertex.Position.x);
                model->boundingBox[1].y = std::min(model->boundingBox[1].y, vertex.Position.y);
                model->boundingBox[1].z = std::min(model->boundingBox[1].z, vertex.Position.z);

                mesh.vertices.push_back(vertex);
            }

            model->meshes.push_back(mesh);
        }

        engine->LoadModel(model);
        State::Models.push_back(model);
    }

    return true;
}

void FixedUpdate(const std::array<bool, 322> &keyMap) {
    float x, y;
    Uint32 mouseState = SDL_GetRelativeMouseState(&x, &y);

    if (!(lastMouseState & SDL_BUTTON_MMASK) && (mouseState & SDL_BUTTON_MMASK) && !State::IsMouseSpamming) {
        engine->SetMouseCaptureState(!State::IsMouseCaptured);
        State::IsMouseCaptured = !State::IsMouseCaptured;
    }
    
    if ((lastMouseState & SDL_BUTTON_MMASK) != (mouseState & SDL_BUTTON_MMASK)) {
        State::IsMouseSpamming = true;
    } else {
        State::IsMouseSpamming = false;
    }

    mouseState &= State::IsMouseCaptured;   // Ignore all input if the mouse is not captured.

    if (State::IsMouseCaptured) {
        if (settings.InvertHorizontal)
            x *= -1;
        if (settings.InvertVertical)
            y *= -1;

        cam.ProcessMouseMovement(x, y);
    }

    if (keyMap[SDL_SCANCODE_W])
        cam.ProcessKeyboard(FORWARD, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_A])
        cam.ProcessKeyboard(LEFT, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_S])
        cam.ProcessKeyboard(BACKWARD, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_D])
        cam.ProcessKeyboard(RIGHT, ENGINE_FIXED_UPDATE_DELTATIME);

    if ((mouseState ^ lastMouseState) & SDL_BUTTON_LMASK) {
        /* Sort by which model is closest to the camera, to enforce the fact that.. */
        /* When the pointer is clicking on an object that has another one behind, The player intends.. */
        /* to click the one closest to them, Because they can't see the one behind. */
        /* Maybe this should be cached, until Models actually gets appended to. */
        std::sort(State::Models.begin(), State::Models.end(), [] (Model *modelOne, Model *modelTwo) { return (glm::distance(cam.Position, modelOne->GetPosition()) > glm::distance(cam.Position, modelTwo->GetPosition())); });
        
        // Deselect last selected object.

        State::CurrentlySelectedObject = nullptr;

        if (State::CurrentlySelectedObjectParticle) {
            engine->RemoveParticle(State::CurrentlySelectedObjectParticle);
            delete State::CurrentlySelectedObjectParticle;

            State::CurrentlySelectedObjectParticle = nullptr;
        }

        for (Model *&model : State::Models) {
            if (intersects(cam.Position, cam.Front, model->boundingBox)) {
                fmt::println("Left mouse button pressed on a model!");
                State::CurrentlySelectedObject = &model;

                State::CurrentlySelectedObjectParticle = new Particle(model->GetPosition(), model->boundingBox[0]);
                engine->AddParticle(State::CurrentlySelectedObjectParticle);

                break;
            }
        }
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

        if (State::CurrentlySelectedObjectParticle) {
            engine->RemoveParticle(State::CurrentlySelectedObjectParticle);
            delete State::CurrentlySelectedObjectParticle;

            State::CurrentlySelectedObjectParticle = nullptr;
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
        std::array<const char *, 2> filterPatterns = {static_cast<const char *>("*.xml"), static_cast<const char *>("*.obj")};
        char *path = tinyfd_openFileDialog("Select the scene to import.", NULL, filterPatterns.size(), filterPatterns.data(), NULL, false);
        if (!path)
            return;

        std::string pathView(path);

        if (endsWith(pathView, ".obj")) {
            Model *model = new Model(pathView);
            engine->LoadModel(model);
            State::Models.push_back(model);
        } else if (endsWith(pathView, ".xml")) {

            try {
                importScene(pathView);
            } catch (const std::runtime_error &e) {
                tinyfd_messageBox("Scene Importer", ("The scene failed to import, Reason: " + std::string(e.what())).c_str(), "ok", "error", 1);
            }
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

        UI::Panel *panel = new UI::Panel(sharedContext, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec4(0.5f, 0.5f, 0.2f, 0.2f));

        UI::Label *label = new UI::Label(sharedContext, "sdl_engine demo", "NotoSans-Black.ttf", glm::vec2(0.0f, 0.0f));

        engine->AddUIPanel(panel);
        engine->AddUILabel(label);

        engine->RegisterUpdateFunction(Update);
        engine->RegisterFixedUpdateFunction(FixedUpdate);

        engine->Start();

        // engine->RemoveUIPanel(panel);

        // delete panel;
        
        // for (Model *&model : State::Models) {
        //     delete model;
        // }

        // if (State::CurrentlySelectedObjectParticle) {
        //     engine->RemoveParticle(State::CurrentlySelectedObjectParticle);
        // }
    } catch(const std::runtime_error &e) {
        fmt::println("Exception has occurred: {}", e.what());
        return -1;
    }

    return 0;
}
