#ifndef SESSION_COMMANDS_HPP
#define SESSION_COMMANDS_HPP

#include <vector>
#include <string>
#include <memory>
#include <cli/cli.h>

#include "context.hpp"

namespace metriffic
{

class session_commands
{    
public:
    session_commands(Context& c);
    std::shared_ptr<cli::Command> create_session_cmd();

private:
    void print_session_usage(std::ostream& out);
    void session_start(std::ostream& out, const std::string& name, const std::string& mode, const std::string& platform);
    void session_stop(std::ostream& out, const std::string& name);
    void session_status(std::ostream& out, const std::string& name);
    
private: 
    Context& m_context;
    std::shared_ptr<cli::Command> m_session_cmd;

private: 
    const std::string CMD_SESSION_NAME = "session";
    const std::string CMD_SESSION_HELP = "Work with session...";
    const std::vector<std::string> CMD_SESSION_PARAMDESC = {
        {"<command>: mandatory parameter, session request to execute. Can be either 'start', 'stop' or 'status'"},
        {"   <mode>: mandatory for 'start' command, specifies the type of session to start."},
        {"   -p|--platform <name platform>: name of the platform to start mission on."},
        {"-n|--name <name of the session>: Name of the session to perform operation on."},
    };
};

} // namespace metriffic

#endif //SESSION_COMMANDS_HPP
