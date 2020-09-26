#ifndef ADMIN_COMMANDS_HPP
#define ADMIN_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>

#include "context.hpp"

namespace metriffic
{

class admin_commands
{    
public:
    admin_commands(Context& c);
    std::shared_ptr<cli::Command> create_admin_cmd();

private:
    void print_admin_usage(std::ostream& out);
    void admin_diagnostics(std::ostream& out);

    void dump_diagnostics(std::ostream& out, const nlohmann::json& msg);

private: 
    Context& m_context;
    std::shared_ptr<cli::Command> m_admin_cmd;
    const std::string CMD_ADMIN_NAME = "admin";
    const std::string CMD_SUB_DIAGNOSTICS = "diagnostics";
    const std::string CMD_ADMIN_HELP = "Administrative access for remote diagnostics and configuration...";
    const std::vector<std::string> CMD_ADMIN_PARAMDESC = {
        {"<subcommand>: mandatory argument, the admin request to execute. Currently only 'diagnostcs' is supported"},
    };
};

} // namespace metriffic

#endif //ADMIN_COMMANDS_HPP
