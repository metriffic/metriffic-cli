#include "admin_commands.hpp"
#include <cxxopts.hpp>
#include <regex>
#include <algorithm>
                        
namespace metriffic
{

template<typename F>
std::shared_ptr<cli::Command> 
create_cmd_helper(const std::string& name,
                  F f,
                  const std::string& help,
                  const std::vector<std::string>& par_desc) 
{
    return std::make_shared<cli::ShellLikeFunctionCommand<F>>(name, f, help, par_desc); 
}

admin_commands::admin_commands(Context& c)
 : m_context(c)
{}

void
admin_commands::print_admin_usage(std::ostream& out)
{
    m_admin_cmd->Help(out);
}


void
admin_commands::admin_diagnostics()
{
    int msg_id = m_context.gql_manager.admin_diagnostics();
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;
    if(show_msg["payload"]["data"] != nullptr) {
        std::cout<<"response: "<<show_msg["payload"]["data"].dump(4)<<std::endl;
    } else 
    if(show_msg["payload"]["errors"] != nullptr ) {
        std::cout<<"Query failed: "<<show_msg["payload"]["errors"][0]["message"]<<std::endl;
    }
}


std::shared_ptr<cli::Command> 
admin_commands::create_admin_cmd()
{
    m_admin_cmd = create_cmd_helper(
        CMD_ADMIN_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_ADMIN_NAME, CMD_ADMIN_HELP);
            options.add_options()
                ("subcommand", CMD_ADMIN_PARAMDESC[0], cxxopts::value<std::string>());

            options.parse_positional({"subcommand"});

            try {
                auto result = options.parse(argc, argv);
                

                if(result.count("subcommand") != 1) {
                    out << CMD_ADMIN_NAME << ": 'subcommand' (currently only '"<<CMD_SUB_DIAGNOSTICS<<"') "
                        << "is a mandatory argument." << std::endl;
                    return;
                }
                auto subcommand = result["subcommand"].as<std::string>();

                if(subcommand == CMD_SUB_DIAGNOSTICS) {
                    admin_diagnostics();
                } else {
                    out << CMD_ADMIN_NAME << ": unsupported subcommand type, "
                        << "the only supported subcommand for now is '"<<CMD_SUB_DIAGNOSTICS<<"'." << std::endl;
                    return;
                }
            } catch (std::exception& e) {
                out << CMD_ADMIN_NAME << ": " << e.what() << std::endl;
                print_admin_usage(out);
                return;
            }        
        },
        CMD_ADMIN_HELP,
        CMD_ADMIN_PARAMDESC
    );
    return m_admin_cmd;
}

} // namespace metriffic
