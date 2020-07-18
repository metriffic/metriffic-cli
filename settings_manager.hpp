#ifndef SETTINGS_MANAGER_HPP
#define SETTINGS_MANAGER_HPP

#include <experimental/filesystem>
#include <nlohmann/json.hpp>

namespace metriffic
{

class settings_manager
{    
public:
    settings_manager();
    std::pair<bool, std::string> workspace(const std::string& username);
    void load();
    void save();

    void create_user(const std::string& username);

private:
    std::experimental::filesystem::path m_path;
    nlohmann::json m_settings;
    const std::string WORKSPACE_TAG = "workspace";
    const std::string USERS_TAG = "users";
    const std::string PATH_TAG = "path";
};

} // namespace metriffic

#endif //SETTINGS_MANAGER_HPP
