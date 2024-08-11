#ifndef SETTINGS_HPP
#define SETTINGS_HPP
#include <string_view>
#include <toml++/toml.hpp>
#include <fmt/core.h>
#include <SDL3/SDL_stdinc.h>

using std::string_view;

class Settings {
public:
// Video
    bool VSyncEnabled;
    Uint32 RenderWidth, RenderHeight;
    Uint32 DisplayWidth, DisplayHeight;
    bool Fullscreen, IgnoreRenderResolution;
    float FieldOfView;
    float CameraNear;

// Profiling information
    bool ReportFPS;

// Input
    float MouseSensitivity;
    float Velocity;
    bool InvertVertical;
    bool InvertHorizontal;

    Settings(const string_view &fileName);

    template<typename T>
    T GetValue(const string_view &name, T def) {
        std::optional<T> ret = m_SettingsTable.at_path(name).value<T>();

        return ret.value_or(def);
    }
    
private:
    toml::table m_SettingsTable;
};

#endif
