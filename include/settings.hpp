#ifndef SETTINGS_HPP
#define SETTINGS_HPP
#include "toml++/toml.hpp"
#include "fmt/core.h"

#include <SDL3/SDL_stdinc.h>
#include <string_view>

class Settings {
public:
// Video
    bool VSyncEnabled;
    Uint32 RenderWidth, RenderHeight;
    Uint32 DisplayWidth, DisplayHeight;
    bool Fullscreen, IgnoreRenderResolution;
    float FieldOfView;

    Settings(const std::string_view fileName);

    template<typename T>
    T GetValue(const std::string_view name, const T& def) {
        const std::optional<T> ret = m_SettingsTable.at_path(name).value<T>();

        return ret.value_or(def);
    }
    
private:
    toml::table m_SettingsTable;
};

#endif
