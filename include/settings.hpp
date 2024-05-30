#ifndef SETTINGS_HPP
#define SETTINGS_HPP
#include <string_view>
#include <toml++/toml.hpp>
#include <fmt/core.h>

using std::string_view;

class Settings {
public:
    bool VSyncEnabled;

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