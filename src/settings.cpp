#include <settings.hpp>

Settings::Settings(const string_view &fileName) {
    try {
        m_SettingsTable = toml::parse_file(fileName);
    } 
    catch (const toml::parse_error &error) {
        throw std::runtime_error(fmt::format("Failed to parse {}, error message: {}.", fileName, error.what()));
    }

    VSyncEnabled = GetValue("video.VSyncEnabled", false);
    Width = GetValue("video.Width", 400);
    Height = GetValue("video.Height", 400);

    ReportFPS = GetValue("profile.ReportFPS", true);

    MouseSensitivity = GetValue("input.MouseSensitivity", 0.1f);
    InvertVertical = GetValue("input.InvertVertical", false);
    InvertHorizontal = GetValue("input.InvertHorizontal", false);
}