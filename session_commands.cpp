#include <cli/cli.h>
#include <cxxopts.hpp>

#include "session_commands.hpp"
#include "context.hpp"

namespace metriffic
{

template<typename F>
std::shared_ptr<cli::Menu> 
create_menu_helper(const std::string& name,
                   F f,
                   const std::string& help,
                   const std::vector<std::string>& par_desc) 
{
    return std::make_shared<cli::ShellLikeFunctionMenu<F>>(name, f, help, par_desc); 
}

session_commands::session_commands(Context& c)
 : m_context(c)
{}

std::shared_ptr<cli::Menu> 
session_commands::create_menu()
{
    m_menu = create_menu_helper(
        CMD_NAME,
        [this](std::ostream& out, int argc, char** argv) { 
            cxxopts::Options options(CMD_NAME, CMD_HELP);
            options.add_options()
                ("name", CMD_PARAMDESC[0], cxxopts::value<std::string>());
            try {
                options.parse_positional({"name"});

                auto result = options.parse(argc, argv);

                if(result.count("name")) {
                    m_name = result["name"].as<std::string>();
                    m_menu->SetSuffix("[" + m_name + "]");    
                    return true;
                } else {
                    out << print_menu_usage() << std::endl;
                    return false;
                }
            } catch (cxxopts::option_not_exists_exception& e) {
                out << CMD_NAME << ": " << e.what() << std::endl;
                print_menu_usage();
                return false;
            }
        },
        CMD_HELP, 
        CMD_PARAMDESC
    );

    m_menu -> Insert(
        CMD_START_NAME,
        [this](std::ostream& out, int argc, char** argv) { 
            this->start(out, argc, argv);
        },
        CMD_START_HELP
    );

    m_menu -> Insert(
        CMD_STOP_NAME,
        [this](std::ostream& out, int argc, char** argv){ 
            this->stop(out, argc, argv);
        },
        "Stop the session" 
    );

    m_menu -> Insert(
        CMD_STATUS_NAME,
        [this](std::ostream& out, int argc, char** argv){ 
            this->status(out, argc, argv);
        },
        "Stop the session" 
    );

    return m_menu;
}

void 
session_commands::start(std::ostream& out, int argc, char** argv) 
{ 
    cxxopts::Options options(CMD_START_NAME, CMD_START_HELP);
    options.add_options()
        ("mode", "Run mode, interactive or batch", cxxopts::value<std::string>())
        ("p, platform", "Platform to run this session on", cxxopts::value<std::string>())
        ("j, jobs", "Target number of simultaneious jobs", cxxopts::value<uint32_t>()->default_value("1"));

    options.parse_positional({"mode"});

    try {
        auto result = options.parse(argc, argv);
        
        if(result.count("mode") != 1) {
            out << CMD_START_NAME << ": mode (either batch or interactive) is a mandatory argument." << std::endl;
            return;
        }
        auto mode = result["mode"].as<std::string>();
        if(mode == "batch") {
            m_mode = "BATCH";
        } else 
        if(mode == "interactive") {
            m_mode = "INTERACTIVE";
        } else {
            out << CMD_START_NAME << ": unsupported mode use 'batch' or 'interactive'." << std::endl;
            return;
        }

        if(result.count("platform") != 1) {
            out << CMD_START_NAME << ": -p|--platform is a mandatory argument." << std::endl;
            return;
        }
        m_platform = result["platform"].as<std::string>();

        m_max_jobs = result["jobs"].as<uint32_t>();

    } catch (cxxopts::option_not_exists_exception& e) {
        out << CMD_START_NAME << ": " << e.what() << std::endl;
        print_start_usage();
        return;
    }

    int msg_id = m_context.gql_manager.session_start(
                                m_name,
                                m_platform,
                                m_mode,
                                m_datasets,
                                m_max_jobs,
                                m_command
                            );
    while(true) {
        auto response = m_context.gql_manager.wait_for_response(msg_id);
        nlohmann::json data_msg = response.second;
        if(data_msg["type"] == "error") {
            out<<"Error: likely an invalid query..."<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["errors"] != nullptr) {
            out<<"Error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            return;
        } else {
            out<<"Bringing up the requested ssh container, this may take a while... (use ctrl-c to cancel) "<<std::endl;
            break;
        }
    }

    msg_id = m_context.gql_manager.subscribe_to_data_stream();
    while(true) {
        auto response = m_context.gql_manager.wait_for_response(msg_id);
        nlohmann::json data_msg = response.second;
        //out<<data_msg.dump(4)<<std::endl;
        if(data_msg["type"] == "error") {
            out<<"Got error in the data stream (abnormal query?)..."<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["errors"] != nullptr) {
            out<<"Error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["data"] != nullptr) {
            //out<<data_msg["payload"]["data"]["subsData"]["message"].get<std::string>()<<std::endl;
            auto msg = nlohmann::json::parse(data_msg["payload"]["data"]["subsData"]["message"].get<std::string>());
            out<<"Container ready, use the following crendials to ssh:"<<std::endl;
            out<<"\thostname:\t"<<msg["host"].get<std::string>()<<".metriffic.com"<<std::endl;            
            out<<"\tssh port:\t"<<msg["port"]<<std::endl;            
            out<<"\tusername:\t"<<msg["username"].get<std::string>()<<std::endl;            
            out<<"\tpassword:\t"<<msg["password"].get<std::string>()<<std::endl;            
            out<<"Note: leaving this session (ctrl-c) will kill the container."<<std::endl;
            break;
        } //else 
        if(response.first) {
            out<<"Got error in the data stream..."<<std::endl;
            break;
        }
    }

}

void 
session_commands::stop(std::ostream& out, int argc, char** argv)
{
    cxxopts::Options options(CMD_STOP_NAME, CMD_STOP_HELP);
    //options.add_options()
    //    ("n, name", "Name of the session to stop", cxxopts::value<std::string>()->default_value("false"));
    try {
        auto result = options.parse(argc, argv);
        int msg_id = m_context.gql_manager.session_stop(result["name"].as<std::string>());
    } catch (cxxopts::option_not_exists_exception& e) {
        out << CMD_STOP_NAME << ": " << e.what() << std::endl;
        print_stop_usage();
    }
    
}  

void 
session_commands::status(std::ostream& out, int argc, char** argv)
{
    cxxopts::Options options(CMD_STATUS_NAME, CMD_STATUS_HELP);
    //options.add_options()
    //    ("n, name", "Name of the session to stop", cxxopts::value<std::string>()->default_value("false"));
    try {
        auto result = options.parse(argc, argv);
    } catch (cxxopts::option_not_exists_exception& e) {
        out << CMD_STATUS_NAME << ": " << e.what() << std::endl;
        print_status_usage();
    }        
    //int msg_id = m_context.gql_manager.session_stop(result["name"].as<std::string>());
}  

std::string 
session_commands::print_menu_usage()
{
    return "Usage: " + CMD_NAME + " " + CMD_PARAMDESC[0]; 
}  

std::string 
session_commands::print_start_usage()
{
    return "Usage: " + CMD_START_NAME; 
}  

std::string 
session_commands::print_stop_usage()
{
    return "Usage: " + CMD_STOP_NAME; 
}  

std::string 
session_commands::print_status_usage()
{
    return "Usage: " + CMD_STATUS_NAME; 
}  

} // namespace metriffic
