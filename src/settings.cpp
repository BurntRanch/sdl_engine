#include <settings.hpp>

Settings::Settings(const string_view &fileName) {
    try {
        m_SettingsTable = toml::parse_file(fileName);
    } 
    catch (const toml::parse_error &error) {
        throw std::runtime_error(fmt::format("Failed to parse {}, error message: {}.", fileName, error.what()));
    }

    VSyncEnabled = GetValue("video.VSyncEnabled", false);
}