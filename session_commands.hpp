#ifndef SESSION_COMMANDS_HPP
#define SESSION_COMMANDS_HPP

#include <vector>
#include <string>
#include <memory>
#include <cli/cli.h>

#include "app_context.hpp"

namespace metriffic
{

class session_commands
{    
public:
    session_commands(app_context& c);
    std::shared_ptr<cli::Command> create_interactive_cmd();
    std::shared_ptr<cli::Command> create_batch_cmd();

private:
    void session_start_batch(std::ostream& out, const std::string& name, const std::string& dockerimage, 
                             const std::string& platform, const std::string& script, int max_jobs,
                             const std::vector<std::string>& datasets);
    void session_start_interactive(std::ostream& out, const std::string& name,
                                   const std::string& dockerimage, const std::string& platform);
    void session_join_interactive(std::ostream& out, const std::string& name);
    void session_stop(std::ostream& out, const std::string& name);
    void session_save(std::ostream& out, const std::string& name, 
                      const std::string& docker_image, const std::string& comment);
    void session_status(std::ostream& out, const std::string& name);

private: 
    app_context& m_context;
    std::string m_last_session_name;

private: 
    const std::string MODE_INTERACTIVE = "interactive";
    const std::string MODE_BATCH       = "batch";

    const std::string CMD_INTERACTIVE_SESSION_NAME = "interactive";
    const std::string CMD_INTERACTIVE_SESSION_HELP = "interactive session management commands...";
    const std::string CMD_BATCH_SESSION_NAME = "batch";
    const std::string CMD_BATCH_SESSION_HELP = "batch session management commands...";
    const std::vector<std::string> CMD_BATCH_PARAMDESC = {
        {"<command>: mandatory argument, session request to execute. Can be either 'start', 'stop', 'list' or 'status'"},
        {"   -p|--platform <platform name>: name of the platform to start mission on."},
        {"   -d|--docker-image <docker image>: docker image to instantiate on the target board."},
        {"   -r|--run-script <script/binary>: the script or binary command to execute, mandatory for batch mode."},
        {"   -i|--input-datasets <[ds1,ds2,...dsn]>: list of input datasets to pass to each instance, mandatory for batch mode."},
        {"   -j|--jobs <N>: maximum number of simultaneous jobs."},
        {"-n|--name <name of the session>: Name of the session to perform operation on."},
    };
    const std::vector<std::string> CMD_INTERACTIVE_PARAMDESC = {
        {"<command>: mandatory argument, session request to execute. Can be either 'start', 'stop', 'join', 'list', 'status' or 'save'"},
        {"   -p|--platform <platform name>: name of the platform to start mission on."},
        {"   -d|--docker-image <docker image>: docker image to instantiate on the target board."},
        {"   -c|--comment <text>: description of the requested operation."},
        {"-n|--name <name of the session>: Name of the session to perform operation on."},
    };
};

} // namespace metriffic

#endif //SESSION_COMMANDS_HPP
