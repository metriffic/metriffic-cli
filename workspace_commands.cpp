#include "workspace_commands.hpp"
#include <cxxopts.hpp>
#include <plog/Log.h>
#include <regex>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

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

workspace_commands::workspace_commands(Context& c)
 : m_context(c)
{}

void
workspace_commands::print_sync_usage(std::ostream& out)
{
    m_sync_cmd->Help(out);
}

std::string 
workspace_commands::build_rsynch_commandline(std::ostream& out,
                                             const std::string& username, 
                                             const std::string& password,
                                             const std::string& dest_host,
                                             unsigned int local_port,
                                             bool enable_delete,
                                             const std::string& direction,
                                             const std::string& user_workspace)
{
    std::stringstream ss;
    // format: "[%t]:%o:%f:Last Modified %M\"
    ss << "rsync -arvz --out-format=\"processing: %f\"  "
        << " -e 'sshpass -p " << password << " ssh -p " << local_port << "'"
        << " --progress ";
    if(enable_delete) {
        ss << " --delete ";
    }               
    if(direction == SYNC_DIR_DOWN) {
        ss << username <<"@localhost: "
            << user_workspace;
    } else 
    if(direction == SYNC_DIR_UP) {
        ss << user_workspace << " "
            << username << "@localhost:";
        //ss << " /home/vazgen/workspace/METRIFFIC/EXPERIMENTAL/remove_it/b/ "
        //   << sync_username << "@" << dest_host << ":/home/ssh_user/bla/";
    } else {
        // shouldn't be possible unless the code is inconsistent... 
        // send out a message. and do nothing.
        out << "error: invalid direction." << std::endl;
    }
    ss << " 2>/dev/null";
    return ss.str();
}

void
workspace_commands::sync(std::ostream& out, 
                         bool enable_delete,
                         const std::string& direction, 
                         const std::string& folder)
{
    out<<"requesting access... ";
    int msg_id = m_context.gql_manager.sync_request();
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    PLOGV << "rsync response: " << std::endl << response.second.dump(4);
    nlohmann::json show_msg = response.second;

    if(show_msg["payload"]["data"] != nullptr) {
        out<<"done."<<std::endl;
        auto sync_username = m_context.username;
        auto sync_password = show_msg["payload"]["data"]["rsyncRequest"].get<std::string>();
        out<<"opening ssh tunnel... ";
        auto tunnel_ret = m_context.ssh.start_rsync_tunnel(sync_username);
        if(tunnel_ret.status) {
            out<<"done."<<std::endl;

            auto workspace = m_context.settings.workspace(sync_username);

            if(workspace.first == false) {
                out << "error: local workspace for the current user doesn't exist." << std::endl;
                return;
            }

            std::string commandline = build_rsynch_commandline(out, sync_username, sync_password,
                                                               tunnel_ret.dest_host, tunnel_ret.local_port,
                                                               enable_delete, direction, workspace.second);
                                                                           
            PLOGV << "rsync commandline: " << commandline;                                                                           
            std::array<char, 32> buffer;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(commandline.c_str(), "r"), pclose);
            if (!pipe) {
                out << "error: failed to start and instance of rsync." << std::endl;
                return;
            }

            std::stringstream rsss;
            int i = 0;
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                std::string strbuf(buffer.data());
                auto len = strlen(buffer.data());
                std::string delimiter = "\r\n";
                std::size_t pos = 0;
                while ((pos = strbuf.find_first_of(delimiter)) != std::string::npos) {

                    std::string full_str = rsss.str() + strbuf.substr(0, pos + 1);
                    std::size_t xpos = full_str.find("(xfr#");
                    std::string filtered_str = full_str.substr(0,xpos);

                    if(strbuf[pos] == '\n') {
                        out << filtered_str;
                        if(xpos != std::string::npos) {
                            out << std::endl;
                        }
                    } else 
                    if(strbuf[pos] == '\r') {                    
                        out << filtered_str << std::flush;
                    }
                    rsss.str("");
                    strbuf.erase(0, pos + 1);
                }
                rsss << strbuf;
            }
            if(m_context.session.RunningCommand()) {
                out<<"sync complete..."<<std::endl;
            } else {
                out<<"sync canceled..."<<std::endl;
            }
        } else {
            out<<"failed."<<std::endl;
        }
        m_context.ssh.stop_rsync_tunnel(sync_username);
        out<<"stopping ssh tunnel. "<<std::endl;
    } else 
    if(show_msg["payload"]["errors"] != nullptr ) {
        out<<"failed."<<std::endl;
        PLOGE << "workspace syncrhonization request failed: " << show_msg["payload"]["errors"].dump(4);
    }
}



std::shared_ptr<cli::Command> 
workspace_commands::create_sync_cmd()
{
    m_sync_cmd = create_cmd_helper(
        CMD_SYNC_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_SYNC_NAME, CMD_SYNC_HELP);
            options.add_options()
                ("direction", CMD_SYNC_PARAMDESC[0], cxxopts::value<std::string>())
                ("f, folder", CMD_SYNC_PARAMDESC[1], cxxopts::value<std::string>())
                ("d, delete", CMD_SYNC_PARAMDESC[2], cxxopts::value<bool>()->default_value("false"));

            options.parse_positional({"direction"});

            try {
                auto result = options.parse(argc, argv);
                if(result.count("direction") != 1) {
                    out << CMD_SYNC_NAME << ": 'direction' (either '"
                        << SYNC_DIR_UP << "' or '" << SYNC_DIR_DOWN
                        << "') is a mandatory argument." << std::endl;
                    return;
                }
                auto direction = result["direction"].as<std::string>();
                if(direction != SYNC_DIR_UP && direction != SYNC_DIR_DOWN) {
                    out << CMD_SYNC_NAME << ": unsupported 'direction', supported directions are '"
                        << SYNC_DIR_UP << "' or '" << SYNC_DIR_DOWN << "'." << std::endl;
                    return;
                }
                
                std::string folder = "";
                if(result.count("folder")) {
                    folder = result["folder"].as<std::string>();
                }

                bool enable_delete = false;                
                if(result.count("delete")) {
                    enable_delete = result["delete"].as<bool>();
                }

                sync(out, enable_delete, direction, folder);

            } catch (std::exception& e) {
                out << CMD_SYNC_NAME << ": " << e.what() << std::endl;
                print_sync_usage(out);
                return;
            }        
        },
        CMD_SYNC_HELP,
        CMD_SYNC_PARAMDESC
    );
    return m_sync_cmd;
}

} // namespace metriffic
