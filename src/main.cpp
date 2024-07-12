#include "camera.hpp"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <chrono>
#include <engine.hpp>
#include <memory>
#include <settings.hpp>
#include <fmt/core.h>
#include <stdexcept>
#include "util.hpp"

std::chrono::high_resolution_clock::time_point lastFixedFrameTime;

Camera cam(glm::vec3(1.0f, 1.0f, 1.0f));
Settings settings("settings.toml");
std::unique_ptr<Engine> engine;

namespace State {
    Model **CurrentlySelectedObject = nullptr;
    std::vector<Model *> Models;

    bool IsMouseCaptured = false;
}

float lastX, lastY;
Uint32 lastMouseState;

void Update() {
    //fmt::println("Hi!");
}

void FixedUpdate(const std::array<bool, 322> &keyMap) {
    float x, y;
    Uint32 mouseState = SDL_GetRelativeMouseState(&x, &y);

    if (!(lastMouseState & SDL_BUTTON_MMASK) && (mouseState & SDL_BUTTON_MMASK)) {
        SDL_SetRelativeMouseMode(!State::IsMouseCaptured);
        State::IsMouseCaptured = !State::IsMouseCaptured;
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
        
        State::CurrentlySelectedObject = nullptr;
        for (Model *&model : State::Models) {
            if (intersects(cam.Position, cam.Front, model->GetBoundingBox())) {
                fmt::println("Left mouse button pressed on a model!");
                State::CurrentlySelectedObject = &model;
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

    engine = std::make_unique<Engine>(settings, &cam);
    
    try {
        engine->Init();

        Model *viking_room = engine->LoadModel("models/viking_room.obj");
        viking_room->SetPosition(glm::vec3(-0.5f, -0.5f, -0.5f));

        State::Models.push_back(viking_room);

        engine->RegisterUpdateFunction(Update);
        engine->RegisterFixedUpdateFunction(FixedUpdate);

        engine->Start();
    } catch(const std::runtime_error &e) {
        fmt::println("Exception has occurred: {}", e.what());
        return -1;
    }
    return 0;
}
