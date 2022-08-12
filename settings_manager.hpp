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
    bool set_workspace(const std::string& username, const std::string& path);
    std::string log_file();

private:
    std::filesystem::path m_path;
    nlohmann::json m_settings;
    const std::string WORKSPACE_TAG = "workspace";
    const std::string USERS_TAG = "users";
    const std::string PATH_TAG = "path";
};

} // namespace metriffic

#endif //SETTINGS_MANAGER_HPP
