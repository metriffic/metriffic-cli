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
    void show_platforms(std::ostream& out);
    void show_docker_images(std::ostream& out, const std::string& platform);
    void show_sessions(std::ostream& out, 
                       const std::string& platform, 
                       const std::vector<std::string>& statuses);
    void show_jobs(std::ostream& out, 
                   const std::string& platform, 
                   const std::string& session);

private: 
    Context& m_context;
    const std::string CMD_SHOW_NAME = "show";
    const std::string CMD_SHOW_HELP = "Query supported platforms/docker-images, show sessions and jobs...";
    const std::vector<std::string> CMD_SHOW_PARAMDESC = {
        {"<items>: mandatory argument, the type of data to show. Can be either 'platforms', 'docker-images', 'sessions', 'jobs'"},
        {"-p|--platform <name of the platform>: platform selector, can be used when querying docker-images, sessions or jobs."},
        {"-s|--session <name of the session>: session selector, can be used when querying jobs."},
        {"-f|--filter <filter in json format>: Used for sessions, format: {state:SUBMITTED|INPROGRESS|CANCELED|FINISHED}."},
    };
};

} // namespace metriffic

#endif //QUERY_COMMANDS_HPP
