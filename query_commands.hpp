#ifndef QUERY_COMMANDS_HPP
#define QUERY_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>

#include "context.hpp"

namespace metriffic
{

class query_commands
{    
public:
    query_commands(Context& c);
    std::shared_ptr<cli::Command> create_show_cmd();

private:
    void print_show_usage(std::ostream& out);
    void show_platforms();
    void show_docker_images(const std::string& platform);
    void show_sessions(const std::string& platform, 
                       const std::vector<std::string>& statuses);
    void show_jobs(const std::string& platform, const std::string& session);

private: 
    Context& m_context;
    std::shared_ptr<cli::Command> m_show_cmd;
    const std::string CMD_SHOW_NAME = "show";
    const std::string CMD_SHOW_HELP = "Query supported platforms/docker-images, show sessions and jobs...";
    const std::vector<std::string> CMD_SHOW_PARAMDESC = {
        {"<items>: mandatory parameter, the type of data to show. Can be either 'platform', 'docker-image', 'session', 'job'"},
        {"-p|--platform <name of the platform>: platform selector, can be used when querying docker-images, sessions or jobs."},
        {"-s|--session <name of the session>: session selector, can be used when querying jobs."},
        {"-f|--filter <filter in json format>: Used for sessions, format: {state:SUBMITTED|INPROGRESS|CANCELED|FINISHED}."},
    };
};

} // namespace metriffic

#endif //QUERY_COMMANDS_HPP