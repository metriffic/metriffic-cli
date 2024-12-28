#include "workspace_commands.hpp"
#include "utils.hpp"
#include <cxxopts.hpp>
#include <plog/Log.h>
#include <regex>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

namespace metriffic
{

workspace_commands::workspace_commands(app_context& c)
 : m_context(c)
{}

std::string 
workspace_commands::build_rsynch_commandline(std::ostream& out,
                                             const std::string& username, 
                                             const std::string& dest_host,
                                             unsigned int local_port,
                                             bool enable_delete,
                                             const std::string& direction,
                                             const std::string& user_workspace,
                                             const std::string& folder)
{
    std::stringstream ss;
    namespace fs = std::filesystem;
    // format: "[%t]:%o:%f:Last Modified %M\"
    ss << "rsync -arvz --out-format=\"processing: %f\"  "
       << " -e 'ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no  -i ~/.config/metriffic/" << username << "/keys/user_key  -p " << local_port << "'"
       << " --progress ";
    if(enable_delete) {
        ss << " --delete ";
    }       
    if(!folder.empty()) {
        ss << "--include='/" << folder << "' --include='/"<<folder<<"/**' --exclude='*' ";
    }        
    if(direction == SYNC_DIR_DOWN) {
        ss << username <<"@localhost: "
           << user_workspace;
    } else 
    if(direction == SYNC_DIR_UP) {
        ss << user_workspace << " "
           << username << "@localhost:";
    } else {
        // shouldn't be possible unless the code is inconsistent... 
        // send out a message. and do nothing.
        out << "error: invalid direction." << std::endl;
    }
    ss << " 2>/dev/null";
    return ss.str();
}


void
workspace_commands::workspace_set(std::ostream& out, const std::string& path)
{
    namespace fs = std::filesystem;

    if(!m_context.is_logged_in()) {
        out << "please log in first." << std::endl;
        return;
    }

    auto fspath = fs::path(path);

    if(!fs::exists(fspath)) {
        if(fs::create_directories(fspath)) { 
            out << "creating folder " << fspath << " for user '" << m_context.username << "'..." << std::endl;
        } else {
            out << "failed to create the specified folder: " << path << "." << std::endl;
            return;
        }
    } else {
        out << "setting existing folder " << fspath << " as a workspace for user '" << m_context.username << "'..." << std::endl;
    }
    m_context.settings.set_workspace(m_context.username, fspath);
}

void
workspace_commands::workspace_show(std::ostream& out)
{
    if(!m_context.is_logged_in()) {
        out << "please log in first." << std::endl;
        return;
    }
    auto ret = m_context.settings.workspace(m_context.username);
    if(ret.first == false) {
        out << "the workspace for user '" << m_context.username << "' is not set..." << std::endl;
    } else {
        out << "the workspace for user '" << m_context.username << "' is currently set to " << ret.second << std::endl;
    }
}

void
workspace_commands::workspace_sync(std::ostream& out, 
                                   bool enable_delete,
                                   const std::string& direction, 
                                   const std::string& folder)
{
    if(!m_context.is_logged_in()) {
        out << "please log in first." << std::endl;
        return;
    }

    out<<"requesting access... ";
    int msg_id = m_context.gql_manager.sync_request();
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    PLOGV << "rsync response: " << std::endl << response.second.dump(4);
    nlohmann::json show_msg = response.second;

    if(show_msg["payload"]["data"] != nullptr) {
        out<<"done."<<std::endl;
        auto sync_username = m_context.username;
        bool status = show_msg["payload"]["data"]["rsyncRequest"].get<bool>();
        out<<"opening ssh tunnel... ";
        auto tunnel_ret = m_context.ssh.start_rsync_tunnel(sync_username, m_context.settings.bastion_key_file(sync_username));
        if(tunnel_ret.status) {
            out<<"done."<<std::endl;

            auto workspace = m_context.settings.workspace(sync_username);

            if(workspace.first == false) {
                out << "error: local workspace for the current user doesn't exist." << std::endl;
                return;
            }
            std::string commandline = build_rsynch_commandline(out, sync_username, 
                                                               tunnel_ret.dest_host, tunnel_ret.local_port,
                                                               enable_delete, direction, workspace.second, folder);
                                                                           
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
    if(show_msg["payload"].contains("errors") ) {
        out<<"failed."<<std::endl;
        PLOGE << "workspace syncrhonization request failed: " << show_msg["payload"]["errors"].dump(4);
    }
}



std::shared_ptr<cli::Command> 
workspace_commands::create_sync_cmd()
{
    return create_cmd_helper(
        CMD_WORKSPACE_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_WORKSPACE_NAME, CMD_WORKSPACE_HELP);
            options.add_options()
                ("command", CMD_WORKSPACE_PARAMDESC[0], cxxopts::value<std::string>())
                ("direction", CMD_WORKSPACE_PARAMDESC[1], cxxopts::value<std::string>())
                ("f, folder", CMD_WORKSPACE_PARAMDESC[2], cxxopts::value<std::string>())
                ("d, delete", CMD_WORKSPACE_PARAMDESC[3], cxxopts::value<bool>()->default_value("false"));

            options.parse_positional({"command", "direction"});

            try {
                auto result = options.parse(argc, argv);
                if(result.count("command") != 1) {
                    out << CMD_WORKSPACE_NAME << ": 'command' (either '"
                        << WORKSPACE_SET_CMD << ", " << WORKSPACE_SHOW_CMD << "' or '" << WORKSPACE_SYNC_CMD
                        << "') is a mandatory argument." << std::endl;
                    return;
                }
                auto command = result["command"].as<std::string>();

                if(command == WORKSPACE_SET_CMD) {
                    if(result.count("folder") != 1) {
                        out << CMD_WORKSPACE_NAME << ": '-f|--folder' is a mandatory argument for 'workspace set'." << std::endl;
                        return;
                    }
                    std::string folder = result["folder"].as<std::string>();
                    workspace_set(out, folder);
                } else 
                if(command == WORKSPACE_SHOW_CMD) {
                    workspace_show(out);
                } else 
                if(command == WORKSPACE_SYNC_CMD) {
                    if(result.count("direction") != 1) {
                        out << CMD_WORKSPACE_NAME << ": 'direction' (either '"
                            << SYNC_DIR_UP << "' or '" << SYNC_DIR_DOWN
                            << "') is a mandatory argument." << std::endl;
                        return;
                    }
                    auto direction = result["direction"].as<std::string>();
                    if(direction != SYNC_DIR_UP && direction != SYNC_DIR_DOWN) {
                        out << CMD_WORKSPACE_NAME << ": unsupported 'direction', supported directions are '"
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
                    workspace_sync(out, enable_delete, direction, folder);

                } else {
                    out << CMD_WORKSPACE_NAME << ": unsupported command, "
                        << "supported types are: '"<< WORKSPACE_SET_CMD << "', '" << WORKSPACE_SHOW_CMD 
                        << "', '" << WORKSPACE_SYNC_CMD << "'." << std::endl;
                    return;
                }

               

            } catch (std::exception& e) {
                out << CMD_WORKSPACE_NAME << ": " << e.what() << std::endl;
                return;
            }        
        },
        [](std::ostream&){},
        CMD_WORKSPACE_HELP,
        CMD_WORKSPACE_PARAMDESC
    );
}

} // namespace metriffic
