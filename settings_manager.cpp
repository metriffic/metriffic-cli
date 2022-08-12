#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "settings_manager.hpp"

namespace fs = std::filesystem;

namespace metriffic
{

settings_manager::settings_manager()
{
    m_settings = {
        {USERS_TAG, {}},
    };

    m_path = fs::path(getenv("HOME")) / ".config" / "metriffic" / "settings";

    if(fs::exists(m_path)) {
        load();
    }  else {
        fs::create_directories(m_path.parent_path()); 
        save();
    }
}

std::pair<bool, std::string>
settings_manager::workspace(const std::string& username)
{
    if(m_settings[USERS_TAG].count(username) == 0 ||
       m_settings[USERS_TAG][username].count(WORKSPACE_TAG) == 0) {
        return {false, ""};
    }
    auto user = m_settings[USERS_TAG][username];
    return {true, user[WORKSPACE_TAG].get<std::string>()};
}

bool
settings_manager::set_workspace(const std::string& username, const std::string& path)
{
    if(m_settings[USERS_TAG].count(username) == 0) {
        return false;
    }
    m_settings[USERS_TAG][username][WORKSPACE_TAG] = path;
    save();
    return true;
}

std::string
settings_manager::log_file()
{
    return m_path.parent_path() / "cli.log";
}


void 
settings_manager::load()
{
    std::ifstream settings_file(m_path);
    m_settings = nlohmann::json::parse(settings_file);
    settings_file.close();
}
void 
settings_manager::save()
{
    std::ofstream settings_file(m_path);
    settings_file << m_settings;
    settings_file.close();
}

void
settings_manager::create_user(const std::string& username)
{
    auto path = m_path.parent_path() / username;
    auto workspace = path / WORKSPACE_TAG;
    fs::create_directories(path.parent_path());
    fs::create_directories(workspace);
    m_settings[USERS_TAG][username][PATH_TAG] = std::string(path) + fs::path::preferred_separator; 
    m_settings[USERS_TAG][username][WORKSPACE_TAG] = std::string(workspace) + fs::path::preferred_separator; 
    save();
}

} // namespace metriffic
