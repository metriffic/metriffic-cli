#ifndef WORKSPACE_COMMANDS_HPP
#define WORKSPACE_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>

#include "app_context.hpp"

namespace metriffic
{

class workspace_commands
{    
public:
    workspace_commands(app_context& c);
    std::shared_ptr<cli::Command> create_sync_cmd();

    void workspace_set(std::ostream& out,
                       const std::string& path);
    void workspace_show(std::ostream& out);
    void workspace_sync(std::ostream& out, 
                        bool enable_delete,
                        const std::string& direction, 
                        const std::string& folder);

private:
    void print_sync_usage(std::ostream& out);
    std::string build_rsynch_commandline(std::ostream& out,
                                         const std::string& username, 
                                         const std::string& dest_host,
                                         unsigned int local_port,
                                         bool enable_delete,
                                         const std::string& direction,
                                         const std::string& user_workspace,
                                         const std::string& folder);

private: 
    app_context& m_context;

    const std::string WORKSPACE_SET_CMD = "set";
    const std::string WORKSPACE_SHOW_CMD = "show";
    const std::string WORKSPACE_SYNC_CMD = "sync";
    
    const std::string SYNC_DIR_UP = "up";
    const std::string SYNC_DIR_DOWN = "down";

    const std::string CMD_WORKSPACE_NAME = "workspace";
    const std::string CMD_WORKSPACE_HELP = "managing workspace";//"synchronize files between the local folder and remote workspace...";
    const std::vector<std::string> CMD_WORKSPACE_PARAMDESC = {
        {"<command>: mandatory argument, workspace command to execute. Can be either 'sync', 'set' or 'show'"},
        {"   <direction>: mandatory for 'sync' command, the direction of file synchronization. Can be either 'up' or 'down'"},
        {"   -f|--folder <name of the local folder>: command option for 'sync' (path to the local subfolder to synchronize) or 'set (new folder for workspace)'."},
        {"   -d|--delete: command option for 'sync', enable deletion of extraneous files from the receiving side."},
    };
};

} // namespace metriffic

#endif //WORKSPACE_COMMANDS_HPP
