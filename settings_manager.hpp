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
    nlohmann::json& operator()();
    void load();
    void save();

    void create_user(const std::string& username);

private:
    std::experimental::filesystem::path m_path;
    nlohmann::json m_settings;
};

} // namespace metriffic

#endif //SETTINGS_MANAGER_HPP
