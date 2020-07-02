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

    std::shared_ptr<cli::Menu> create_menu();

    void start(std::ostream& out, int argc, char** argv);

    void stop(std::ostream& out, int argc, char** argv);

    void status(std::ostream& out, int argc, char** argv);

private:
    std::string print_menu_usage();
    std::string print_start_usage();
    std::string print_stop_usage();
    std::string print_status_usage();

    std::string m_name;
    std::string m_platform;
    std::string m_mode;
    std::string m_command;
    int m_max_jobs;
    std::vector<std::string> m_datasets;

private: 
    Context& m_context;
    std::shared_ptr<cli::Menu> m_menu;

private: 
    const std::string CMD_NAME = "session";
    const std::string CMD_HELP = "Session management.";
    const std::vector<std::string> CMD_PARAMDESC = {
        {"-n|--name <name of the session to start or switch to>"}
    };

    const std::string CMD_START_NAME = "start";
    const std::string CMD_START_HELP = "Start the session.";
    const std::string CMD_STOP_NAME = "stop";
    const std::string CMD_STOP_HELP = "Stop the current session.";
    const std::string CMD_STATUS_NAME = "status";
    const std::string CMD_STATUS_HELP = "Get the status of the session.";
};

} // namespace metriffic

#endif //SESSION_COMMANDS_HPP
