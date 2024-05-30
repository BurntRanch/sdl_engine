#include <engine.hpp>
#include <memory>
#include <settings.hpp>
#include <fmt/core.h>
#include <boost/thread.hpp>
#include <stdexcept>

int main() {
    std::unique_ptr<Engine> engine = std::make_unique<Engine>();

    Settings settings("settings.toml");

    engine->settings = &settings;
    
    try {
        engine->Init();

        Model *viking_room = engine->LoadModel("models/viking_room.obj");
        viking_room->Position = glm::vec3(0.5f, 0.5f, 0.5f);

        boost::thread thread(boost::bind(&Engine::Start, engine.get()));
        thread.join();
    } catch(std::runtime_error e) {
        fmt::println("Exception has occurred: {}", e.what());
        return -1;
    }
    return 0;
}
