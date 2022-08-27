#ifndef SETTINGS_MANAGER_HPP
#define SETTINGS_MANAGER_HPP

#include <filesystem>
#include <nlohmann/json.hpp>

namespace metriffic
{

class settings_manager
{    
public:
    settings_manager();

    void load();
    void save();
    void create_user(const std::string& username);

    // accessors
    std::pair<bool, std::string> workspace(const std::string& username);
    std::pair<std::string, std::string> active_user();
    bool user_config_exists(const std::string& username);
    std::string log_file();
    // mutators
    bool set_workspace(const std::string& username, const std::string& path);
    bool set_active_user(const std::string& username, const std::string& token);
    void clear_active_user();

private:
    std::filesystem::path m_path;
    nlohmann::json m_settings;
    const std::string WORKSPACE_TAG = "workspace";
    const std::string USERS_TAG = "users";
    const std::string ACTIVE_USER_TAG = "active_user";
    const std::string PATH_TAG = "path";
};

} // namespace metriffic

#endif //SETTINGS_MANAGER_HPP
