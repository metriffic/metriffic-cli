#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "settings_manager.hpp"

namespace fs = std::experimental::filesystem;

namespace metriffic
{

settings_manager::settings_manager()
{
    m_settings = {
        {"users", {}},
    };

    m_path = fs::path(getenv("HOME")) / ".config" / "metriffic" / "settings";

    if(fs::exists(m_path)) {
        load();
    }  else {
        fs::create_directories(m_path.parent_path()); 
        save();
    }
}

nlohmann::json& 
settings_manager::operator()()
{
    return m_settings;
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
    fs::create_directories(path.parent_path());
    m_settings["users"][username]["path"] = path; 
    save();
}

} // namespace metriffic
