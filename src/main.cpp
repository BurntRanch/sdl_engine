#include "camera.hpp"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
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

Model *viking_room;

float lastX, lastY;

void Update() {
    //fmt::println("Hi!");
}

void FixedUpdate(const std::array<bool, 322> &keyMap) {
    float x, y;
    Uint32 mouseState = SDL_GetRelativeMouseState(&x, &y);

    if (settings.InvertHorizontal)
        x *= -1;
    if (settings.InvertVertical)
        y *= -1;

    cam.ProcessMouseMovement(x, y);

    if (keyMap[SDL_SCANCODE_W])
        cam.ProcessKeyboard(FORWARD, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_A])
        cam.ProcessKeyboard(LEFT, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_S])
        cam.ProcessKeyboard(BACKWARD, ENGINE_FIXED_UPDATE_DELTATIME);
    if (keyMap[SDL_SCANCODE_D])
        cam.ProcessKeyboard(RIGHT, ENGINE_FIXED_UPDATE_DELTATIME);

    if (mouseState & SDL_BUTTON_LMASK) {
        if (intersects(cam.Position, cam.Front, viking_room->GetBoundingBox())) {
            fmt::println("Left mouse button pressed on the viking room!");
        }
    }

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

    Engine engine(settings, &cam);
    
    try {
        engine.Init();

        viking_room = engine.LoadModel("models/viking_room.obj");
        viking_room->SetPosition(glm::vec3(-0.5f, -0.5f, -0.5f));

        engine.RegisterUpdateFunction(Update);
        engine.RegisterFixedUpdateFunction(FixedUpdate);

        engine.Start();
    } catch(const std::runtime_error &e) {
        fmt::println("Exception has occurred: {}", e.what());
        return -1;
    }
    return 0;
}
