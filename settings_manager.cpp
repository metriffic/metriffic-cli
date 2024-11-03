#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "settings_manager.hpp"
#include "key_generator.hpp"

namespace fs = std::filesystem;

namespace metriffic
{

settings_manager::settings_manager()
{
    m_settings = {
        {USERS_TAG, {}},
        {ACTIVE_USER_TAG, {}}
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

std::pair<std::string, std::string> 
settings_manager::active_user()
{
    const auto& user_info = m_settings[ACTIVE_USER_TAG];        
    if(!user_info.is_null()) {
        return std::make_pair(user_info["username"], user_info["token"]);
    } else {
        return std::make_pair("","");
    }
}

bool
settings_manager::set_active_user(const std::string& username, const std::string& token)
{
    m_settings[ACTIVE_USER_TAG]["username"] = username;
    m_settings[ACTIVE_USER_TAG]["token"] = token;
    save();
    return true;
}

void
settings_manager::clear_active_user()
{
    m_settings.erase(ACTIVE_USER_TAG);
    save();
}

bool
settings_manager::user_config_exists(const std::string& username)
{
    return m_settings[USERS_TAG].count(username);
}

std::string
settings_manager::log_file()
{
    return m_path.parent_path() / "cli.log";
}

std::string
settings_manager::bastion_key_file(const std::string& username)
{
    return m_path.parent_path() / username / KEYS_TAG / "bastion_key";
}

std::string
settings_manager::user_key_file(const std::string& username)
{
    return m_path.parent_path() / username / KEYS_TAG / "user_key";
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
    settings_file << std::setw(2) << m_settings;
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

std::tuple<bool, std::string> 
settings_manager::generate_keys(const std::string& username)
{
    auto path = m_path.parent_path() / username / KEYS_TAG;
    fs::create_directories(path);

    const std::string bastion_key = bastion_key_file(username);
    metriffic::key_generator::generate_key_pair(bastion_key, 1024, username);
    const std::string user_key = user_key_file(username);
    metriffic::key_generator::generate_key_pair(user_key, 1024, "root@localhost");

    return std::make_tuple(true, "");
}
} // namespace metriffic
