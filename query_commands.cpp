#include "query_commands.hpp"
#include <cxxopts.hpp>
#include <regex>
#include <algorithm>
                        
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

query_commands::query_commands(Context& c)
 : m_context(c)
{}

void
query_commands::print_show_usage(std::ostream& out)
{
    m_show_cmd->Help(out);
}

void
query_commands::show_platforms()
{
    int msg_id = m_context.gql_manager.query_platforms();
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;

    if(show_msg["payload"]["data"] != nullptr) {
        for (auto& s : show_msg["payload"]["data"]["allPlatforms"]) {
            std::cout << "  " << s["name"].get<std::string>() 
                      << ", " << s["description"].get<std::string>() << std::endl;
        }
    } else 
    if(show_msg["payload"]["errors"] != nullptr ) {
        std::cout<<"Query failed: "<<show_msg["payload"]["errors"][0]["message"]<<std::endl;
    }
}

void
query_commands::show_docker_images(const std::string& platform)
{
    int msg_id = m_context.gql_manager.query_docker_images(platform);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;
    //std::cout<<response.second.dump(4)<<std::endl;
    if(show_msg["payload"]["data"] != nullptr) {
        for (auto& s : show_msg["payload"]["data"]["allDockerImages"]) {
            std::cout << "  " << s["name"].get<std::string>() 
                      << ", platform: " << s["platform"]["name"].get<std::string>() << std::endl;
        }
    } else 
    if(show_msg["payload"]["errors"] != nullptr ) {
        std::cout<<"Query failed: "<<show_msg["payload"]["errors"][0]["message"]<<std::endl;
    }
}

void
query_commands::show_sessions(const std::string& platform, 
                              const std::vector<std::string>& statuses)
{
    int msg_id = m_context.gql_manager.query_sessions(platform, statuses);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    //std::cout<<response.second.dump(4)<<std::endl;
    nlohmann::json show_msg = response.second;
    if(show_msg["payload"]["data"] != nullptr) {
        for (auto& s : show_msg["payload"]["data"]["allSessions"]) {
            std::cout << "  " << s["name"].get<std::string>() 
                      << "\t : " << s["state"].get<std::string>() << std::endl;
        }
    } else 
    if(show_msg["payload"]["errors"] != nullptr ) {
        std::cout<<"Query failed: "<<show_msg["payload"]["errors"][0]["message"]<<std::endl;
    }
}

void
query_commands::show_jobs(const std::string& platform, const std::string& session)
{
    int msg_id = m_context.gql_manager.query_jobs(platform, session);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;
    if(show_msg["payload"]["data"] != nullptr) {
        std::cout<<"response: "<<show_msg["payload"]["data"]<<std::endl;
    } else 
    if(show_msg["payload"]["errors"] != nullptr ) {
        std::cout<<"Query failed: "<<show_msg["payload"]["errors"][0]["message"]<<std::endl;
    }
}


std::shared_ptr<cli::Command> 
query_commands::create_show_cmd()
{
    m_show_cmd = create_cmd_helper(
        CMD_SHOW_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_SHOW_NAME, CMD_SHOW_HELP);
            options.add_options()
                ("items", CMD_SHOW_PARAMDESC[0], cxxopts::value<std::string>())
                ("p, platform", CMD_SHOW_PARAMDESC[1], cxxopts::value<std::string>())
                ("s, session", CMD_SHOW_PARAMDESC[2], cxxopts::value<std::string>())
                ("f, filter", CMD_SHOW_PARAMDESC[3], cxxopts::value<std::string>());

            options.parse_positional({"items"});

            try {
                auto result = options.parse(argc, argv);
                

                if(result.count("items") != 1) {
                    out << CMD_SHOW_NAME << ": 'items' (either 'platforms', 'docker-images', 'sessions' or 'jobs') "
                        << "is a mandatory argument." << std::endl;
                    return;
                }
                auto items = result["items"].as<std::string>();

                std::vector<std::string> filter;
                if(result.count("filter")) {
                    if(items != "sessions") {
                        out << CMD_SHOW_NAME << ": Option --filter can be used when querying sessions only."<< std::endl;
                        return;
                    }
                    std::string str_filt = result["filter"].as<std::string>();
                    str_filt.erase(std::remove(str_filt.begin(), str_filt.end(), ' '), str_filt.end());
    
                    std::regex reg("[:|]+");
                    std::sregex_token_iterator begin(str_filt.begin(), str_filt.end(), reg, -1);
                    std::sregex_token_iterator end;
                    std::vector<std::string> parsed_filt(begin, end);
                    if(parsed_filt.size() < 2 ||  parsed_filt[0] != "status") {
                        out << CMD_SHOW_NAME << ": The argument for --filter must be in status:<A|B|...> format."<< std::endl;
                        return;
                    }
                    filter.assign(parsed_filt.begin()+1, parsed_filt.end());
                }
                std::string platform = "";
                if(result.count("platform")) {
                    platform = result["platform"].as<std::string>();
                }

                if(items == "platforms") {
                    show_platforms();
                } else 
                if(items == "docker-images") {
                    show_docker_images(platform);
                } else 
                if(items == "sessions") {
                    show_sessions(platform, filter);
                } else 
                if(items == "jobs") {
                    std::string platform;
                    std::string session;
                    show_jobs(platform, session);
                } else {
                    out << CMD_SHOW_NAME << ": Unsupported item type. "
                        << "Supported types are: 'platforms', 'docker-images', 'sessions', 'jobs'." << std::endl;
                    return;
                }
            } catch (std::exception& e) {
                out << CMD_SHOW_NAME << ": " << e.what() << std::endl;
                print_show_usage(out);
                return;
            }        
        },
        CMD_SHOW_HELP,
        CMD_SHOW_PARAMDESC
    );
    return m_show_cmd;
}

} // namespace metriffic
