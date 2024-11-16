#ifndef ADMIN_COMMANDS_HPP
#define ADMIN_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>

#include "app_context.hpp"

namespace metriffic
{

class admin_commands
{    
public:
    admin_commands(app_context& c);
    std::shared_ptr<cli::Command> create_admin_cmd();
    std::shared_ptr<cli::Command> create_register_cmd();

private:
    void admin_diagnostics(std::ostream& out);
    void dump_diagnostics(std::ostream& out, const nlohmann::json& msg);

    void admin_register(std::ostream& out);
    void initialize_new_user(const std::string& username);

private: 
    app_context& m_context;
    const std::string CMD_ADMIN_NAME = "admin";
    const std::string CMD_SUB_DIAGNOSTICS = "diagnostics";
    const std::string CMD_SUB_REGISTER = "register";
    const std::string CMD_ADMIN_HELP = "Administrative access for remote diagnostics and configuration...";
    const std::vector<std::string> CMD_ADMIN_PARAMDESC = {
        {"<subcommand>: mandatory argument, the admin request to execute. Currently supporting 'diagnostcs' and 'register'..."},
    };
};

} // namespace metriffic

#endif //ADMIN_COMMANDS_HPP
