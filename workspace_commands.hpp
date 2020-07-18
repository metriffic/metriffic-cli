#ifndef WORKSPACE_COMMANDS_HPP
#define WORKSPACE_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>

#include "context.hpp"

namespace metriffic
{

class workspace_commands
{    
public:
    workspace_commands(Context& c);
    std::shared_ptr<cli::Command> create_sync_cmd();

    void sync(std::ostream& out, 
              bool enable_delete,
              const std::string& direction, 
              const std::string& folder);

private:
    void print_sync_usage(std::ostream& out);
    std::string build_rsynch_commandline(std::ostream& out,
                                         const std::string& username, 
                                         const std::string& password,
                                         const std::string& dest_host,
                                         unsigned int local_port,
                                         bool enable_delete,
                                         const std::string& direction,
                                         const std::string& user_workspace);

private: 
    Context& m_context;
    std::shared_ptr<cli::Command> m_sync_cmd;
    
    const std::string SYNC_DIR_UP = "up";
    const std::string SYNC_DIR_DOWN = "down";

    const std::string CMD_SYNC_NAME = "sync";
    const std::string CMD_SYNC_HELP = "synchronize files between the local folder and remote workspace...";
    const std::vector<std::string> CMD_SYNC_PARAMDESC = {
        {"<direction>: optional parameter, the direction of file synchronization. Can be either 'up' or 'down'"},
        {"-f|--folder <name of the local folder>: name of the local root folder to synchronize."},
        {"-d|--delete: enable deletion of extraneous files from the receiving side."},
    };
};

} // namespace metriffic

#endif //WORKSPACE_COMMANDS_HPP
