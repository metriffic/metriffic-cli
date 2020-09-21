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
    void session_start_batch(std::ostream& out, const std::string& name, const std::string& dockerimage, 
                             const std::string& platform, const std::string& script, int max_jobs,
                             const std::vector<std::string>& datasets);
    void session_start_interactive(std::ostream& out, const std::string& name,
                                   const std::string& dockerimage, const std::string& platform);
    void session_stop(std::ostream& out, const std::string& name);
    void session_save(std::ostream& out, const std::string& name, 
                      const std::string& docker_image, const std::string& comment);
    void session_status(std::ostream& out, const std::string& name);
    
private: 
    Context& m_context;
    std::shared_ptr<cli::Command> m_session_cmd;

private: 
    const std::string MODE_INTERACTIVE = "interactive";
    const std::string MODE_BATCH       = "batch";

    const std::string CMD_SESSION_NAME = "session";
    const std::string CMD_SESSION_HELP = "session management commands...";
    const std::vector<std::string> CMD_SESSION_PARAMDESC = {
        {"<command>: mandatory argument, session request to execute. Can be either 'start', 'stop', 'status' or 'save'"},
        {"   <mode>: mandatory for 'start' command, specifies the type of session to start."},
        {"   -p|--platform <platform name>: name of the platform to start mission on."},
        {"   -d|--docker-image <docker image>: docker image to instantiate on the target board."},
        {"   -r|--run-script <script/binary>: the script or binary command to execute, mandatory for batch mode."},
        {"   -i|--input-datasets <[ds1,ds2,...dsn]>: list of input datasets to pass to each instance, mandatory for batch mode."},
        {"   -j|--jobs <N>: maximum number of simultaneous jobs."},
        {"   -c|--comment <text>: description of the requested operation."},
        {"-n|--name <name of the session>: Name of the session to perform operation on."},
    };
};

} // namespace metriffic

#endif //SESSION_COMMANDS_HPP
