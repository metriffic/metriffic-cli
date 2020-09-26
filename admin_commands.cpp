#include "admin_commands.hpp"
#include <cxxopts.hpp>
#include <plog/Log.h>
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
admin_commands::dump_diagnostics(std::ostream& out, const nlohmann::json& msg)
{
    out << std::endl << "printing platform diagnostics:" << std::endl;
    for (const auto& pel : msg["platforms"].items()) {
        const auto& p = pel.value();
        out << "    platform: " << p["name"].get<std::string>() << std::endl;
        for (const auto& bel : p["boards"].items()) {
            const auto& b = bel.value();
            out << "        hostname: " << b["hostname"].get<std::string>() << std::endl;
            out << "           status: " << (b["alive"] ? "alive" : "dead") << std::endl;
            out << "           ping: " << b["ping"].get<std::string>() << std::endl;
            out << "           used: " << (b["used"] ? "yes" : "no") << std::endl;
        }
    }
    
    out << std::endl << "printing session diagnostics:" << std::endl;    
    for (const auto& pel : msg["sessions"].items()) {
        const auto& p = pel.value();
        out << "    platform: " << p["name"].get<std::string>() << std::endl;    
        out << "      submitted sessions:" << std::endl;
        for (const auto& sel : p["sessions"].items()) {
            const auto& s = sel.value();
            out << "        session: " << s["name"].get<std::string>() << std::endl;
            out << "           total jobs: " << s["total_jobs"].get<int>() << std::endl;
            out << "           running jobs: " << s["running_jobs"].get<int>() << std::endl;
            out << "           remaining jobs: " << s["remaining_jobs"].get<int>() << std::endl;
        }
        out << "      running jobs:" << std::endl;
        for (const auto& rel : p["running_jobs"].items()) {
            const auto& r = rel.value();
            out << "      job: " << r["name"].get<std::string>() << std::endl;
            out << "           type: " << r["type"].get<std::string>() << std::endl;
            out << "           start: " << r["start"].get<std::string>() <<std::endl;
            out << "           container: " << r["container"].get<std::string>() << std::endl;
            out << "           hostname: " << r["board"].get<std::string>() << std::endl;
        }
    }

}

void
admin_commands::admin_diagnostics(std::ostream& out)
{
    int sbs_msg_id = m_context.gql_manager.subscribe_to_data_stream();
    int msg_id = m_context.gql_manager.admin_diagnostics();

    while(true) {
        auto response = m_context.gql_manager.wait_for_response({msg_id, sbs_msg_id});
        if(response.first) {
            out << "interrupted..." << std::endl;
            break;
        }

        for(const auto& data_msg : response.second ) {
            PLOGV << "admin diagnostics response: " << data_msg.dump(4);

            if(!data_msg.contains("payload")) {
                continue;
            }
            if(data_msg["type"] == "error") {
                out << "datastream error (abnormal query?)..." << std::endl;
                return;
            } else 
            if(data_msg["payload"].contains("errors")) {
                out << "error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
                return;
            }
            if(data_msg["id"] == sbs_msg_id) {
                if(data_msg["payload"].contains("data")) {
                    auto msg = nlohmann::json::parse(data_msg["payload"]["data"]["subsData"]["message"].get<std::string>());
                    dump_diagnostics(out, msg);
                    return;
                } else {
                    out << "missing diagnostics data (communcation error?)..." << std::endl;
                }
            }
        }
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
                    admin_diagnostics(out);
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
