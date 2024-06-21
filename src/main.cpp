#include "camera.hpp"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <chrono>
#include <engine.hpp>
#include <memory>
#include <settings.hpp>
#include <fmt/core.h>
#include <stdexcept>

std::chrono::high_resolution_clock::time_point lastFixedFrameTime;

Camera cam(glm::vec3(1.0f, 1.0f, 1.0f));
Settings settings("settings.toml");

float lastX, lastY;

void Update() {
    //fmt::println("Hi!");
}

inline void FixedUpdate(std::array<bool, 322> keyMap) {
    float x, y;
    SDL_GetRelativeMouseState(&x, &y);

    if (settings.InvertHorizontal)
        x *= -1;
    if (settings.InvertVertical)
        y *= -1;

    cam.ProcessMouseMovement(x, y);

    // lastX = x;
    // lastY = y;

    if (keyMap[SDL_SCANCODE_W])
        cam.ProcessKeyboard(FORWARD, 1.0f/60);
    if (keyMap[SDL_SCANCODE_A])
        cam.ProcessKeyboard(LEFT, 1.0f/60);
    if (keyMap[SDL_SCANCODE_S])
        cam.ProcessKeyboard(BACKWARD, 1.0f/60);
    if (keyMap[SDL_SCANCODE_D])
        cam.ProcessKeyboard(RIGHT, 1.0f/60);
}

int main() {
    cam.MouseSensitivity = settings.MouseSensitivity;

    Engine engine(settings, &cam);
    
    try {
        engine.Init();

        Model *viking_room = engine.LoadModel("models/viking_room.obj");
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
