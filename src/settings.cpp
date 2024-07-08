#include <settings.hpp>

Settings::Settings(const string_view &fileName) {
    try {
        m_SettingsTable = toml::parse_file(fileName);
    } 
    catch (const toml::parse_error &error) {
        throw std::runtime_error(fmt::format("Failed to parse {}, error message: {}.", fileName, error.what()));
    }

    VSyncEnabled = GetValue("video.VSyncEnabled", false);
    RenderWidth = GetValue("video.RenderWidth", 800);
    RenderHeight = GetValue("video.RenderHeight", 600);
    DisplayWidth = GetValue("video.DisplayWidth", 400);
    DisplayHeight = GetValue("video.DisplayHeight", 400);
    Fullscreen = GetValue("video.Fullscreen", false);
    IgnoreRenderResolution = GetValue("video.IgnoreRenderResolution", false);
    FieldOfView = GetValue("video.FieldOfView", FIELDOFVIEW);

    ReportFPS = GetValue("profile.ReportFPS", true);

    MouseSensitivity = GetValue("input.MouseSensitivity", 0.1f);
    InvertVertical = GetValue("input.InvertVertical", false);
    InvertHorizontal = GetValue("input.InvertHorizontal", false);
}