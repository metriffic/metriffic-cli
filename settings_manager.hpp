#ifndef SETTINGS_MANAGER_HPP
#define SETTINGS_MANAGER_HPP

#include <filesystem>
#include <tuple>
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
    std::tuple<bool, std::string> generate_keys(const std::string& username);

    // accessors
    std::pair<bool, std::string> workspace(const std::string& username);
    bool user_config_exists(const std::string& username);
    std::string log_file();
    std::string bastion_key_file(const std::string& username);
    std::string user_key_file(const std::string& username);
    // mutators
    bool set_workspace(const std::string& username, const std::string& path);

private:
    std::filesystem::path m_path;
    nlohmann::json m_settings;
    const std::string WORKSPACE_TAG = "workspace";
    const std::string USERS_TAG = "users";
    const std::string KEYS_TAG = "keys";
    const std::string PATH_TAG = "path";
};

} // namespace metriffic

#endif //SETTINGS_MANAGER_HPP
