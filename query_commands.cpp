#include "query_commands.hpp"
#include "utils.hpp"

#include <cxxopts.hpp>
#include <termcolor/termcolor.hpp>
#include <regex>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <map>
#include <list>

namespace metriffic
{

namespace tc = termcolor;

query_commands::query_commands(app_context& c)
 : m_context(c)
{}

void
query_commands::show_platforms(std::ostream& out)
{
    int msg_id = m_context.gql_manager.query_platforms();
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;

    if(show_msg["payload"]["data"] != nullptr) {
        for (auto& s : show_msg["payload"]["data"]["allPlatforms"]) {

            out << "    " <<  tc::bold << tc::underline << tc::cyan  << s["name"].get<std::string>() << tc::reset 
                << ",  " << s["description"].get<std::string>() << std::endl;
        }
    } else 
    if(show_msg["payload"].contains("errors") ) {
        out<<"error: "<<show_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
    }
}

void
query_commands::show_docker_images(std::ostream& out, const std::string& platform)
{
    int msg_id = m_context.gql_manager.query_docker_images(platform);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;
    //std::cout<<response.second.dump(4)<<std::endl;
    if(show_msg["payload"]["data"] != nullptr) {
        std::map<std::string, std::list<std::string>> platform_images;
        for(auto& s : show_msg["payload"]["data"]["allDockerImages"]) {
            platform_images[s["platform"]["name"].get<std::string>()].push_back(s["name"].get<std::string>());
        }

        for(auto& pi : platform_images) {
            out << "    platform: " << tc::bold << tc::underline << tc::cyan << pi.first << tc::reset << std::endl;
            for (const auto& di : pi.second) {
                out << "        " << tc::bold << di << tc::reset << std::endl;
            }
        }
    } else 
    if(show_msg["payload"].contains("errors") ) {
        out << "error: " << show_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
    }
}

std::string format_timestamp(time_t timestamp) {
    std::tm* time_info = std::localtime(&timestamp);
    if (time_info == nullptr) {
        return "invalid timestamp"; 
    }

    std::stringstream ss;
    ss << std::put_time(time_info, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void
query_commands::show_sessions(std::ostream& out, 
                              const std::string& platform, 
                              const std::vector<std::string>& statuses)
{
    int msg_id = m_context.gql_manager.query_sessions(platform, statuses);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;
    if(show_msg["payload"]["data"] != nullptr) {
        for (auto& s : show_msg["payload"]["data"]["allSessions"]) {
            out << "  name: " << s["name"].get<std::string>() 
                      << "\t status: " << s["state"].get<std::string>() 
                      << "\t created: " << format_timestamp(s["createdAt"].get<int>()) 
                      << std::endl;
        }
    } else 
    if(show_msg["payload"].contains("errors") ) {
        out<<"error: "<<show_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
    }
}

/*void
query_commands::show_jobs(std::ostream& out, const std::string& platform, const std::string& session)
{
    int msg_id = m_context.gql_manager.query_jobs(platform, session);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json show_msg = response.second;
    if(show_msg["payload"]["data"] != nullptr) {
        out<<"response: "<<show_msg["payload"]["data"]<<std::endl;
    } else 
    if(show_msg["payload"].contains("errors") ) {
        out<<"error: "<<show_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
    }
}*/


std::shared_ptr<cli::Command> 
query_commands::create_show_cmd()
{
    return create_cmd_helper(
        CMD_SHOW_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_SHOW_NAME, CMD_SHOW_HELP);
            options.add_options()
                ("items", CMD_SHOW_PARAMDESC[0], cxxopts::value<std::string>())
                ("p, platform", CMD_SHOW_PARAMDESC[1], cxxopts::value<std::string>())
                ("s, session", CMD_SHOW_PARAMDESC[2], cxxopts::value<std::string>());
                //("f, filter", CMD_SHOW_PARAMDESC[3], cxxopts::value<std::string>());

            options.parse_positional({"items"});

            try {
                auto result = options.parse(argc, argv);
                

                if(result.count("items") != 1) {
                    out << CMD_SHOW_NAME << ": 'items' (either 'platforms' or 'docker-images') "
                        << "is a mandatory argument." << std::endl;
                    return;
                }
                auto items = result["items"].as<std::string>();

                std::vector<std::string> filter;
                if(result.count("filter")) {
                    if(items != "sessions") {
                        out << CMD_SHOW_NAME << ": option --filter can be used when querying sessions only."<< std::endl;
                        return;
                    }
                    std::string str_filt = result["filter"].as<std::string>();
                    str_filt.erase(std::remove(str_filt.begin(), str_filt.end(), ' '), str_filt.end());
    
                    std::regex reg("[:|]+");
                    std::sregex_token_iterator begin(str_filt.begin(), str_filt.end(), reg, -1);
                    std::sregex_token_iterator end;
                    std::vector<std::string> parsed_filt(begin, end);
                    if(parsed_filt.size() < 2 ||  parsed_filt[0] != "status") {
                        out << CMD_SHOW_NAME << ": the argument for --filter must be in status:<A|B|...> format."<< std::endl;
                        return;
                    }
                    filter.assign(parsed_filt.begin()+1, parsed_filt.end());
                }
                std::string platform = "";
                if(result.count("platform")) {
                    platform = result["platform"].as<std::string>();
                }

                if(items == "platforms") {
                    show_platforms(out);
                } else 
                if(items == "docker-images") {
                    show_docker_images(out, platform);
                } else 
                if(items == "sessions") {
                    show_sessions(out, platform, filter);
                // } else 
                // if(items == "jobs") {
                //     std::string platform;
                //     std::string session;
                //     show_jobs(out, platform, session);
                } else {
                    out << CMD_SHOW_NAME << ": unsupported item type, "
                        << "supported types are: 'platforms' or 'docker-images'." << std::endl;
                    return;
                }
            } catch (std::exception& e) {
                out << CMD_SHOW_NAME << ": " << e.what() << std::endl;
                return;
            }        
        },
        [](std::ostream&){},
        CMD_SHOW_HELP,
        CMD_SHOW_PARAMDESC
    );
}

} // namespace metriffic
