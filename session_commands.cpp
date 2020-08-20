#include <regex>
#include <cli/cli.h>
#include <cxxopts.hpp>
#include <plog/Log.h>

#include "session_commands.hpp"
#include "context.hpp"

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

session_commands::session_commands(Context& c)
 : m_context(c)
{}

void
session_commands::print_session_usage(std::ostream& out)
{
    m_session_cmd->Help(out);
}

void
session_commands::session_start_batch(std::ostream& out, const std::string& name, const std::string& dockerimage, 
                                      const std::string& platform, const std::string& script, int max_jobs, 
                                      const std::vector<std::string>& datasets)
{
    int msg_id = m_context.gql_manager.session_start(
                                name,
                                platform,
                                MODE_BATCH,
                                dockerimage,
                                datasets,
                                max_jobs,
                                script);
    while(true) {
        auto response = m_context.gql_manager.wait_for_response(msg_id);
        nlohmann::json data_msg = response.second;
        if(data_msg["type"] == "error") {
            out<<"error: likely an invalid query..."<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["errors"] != nullptr) {
            out<<"error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            return;
        } else {
            break;
        }
    }
}

void
session_commands::session_start_interactive(std::ostream& out, const std::string& name, 
                                            const std::string& dockerimage, const std::string& platform)
{
    const int MAX_JOBS = 1;    
    int msg_id = m_context.gql_manager.session_start(
                                name,
                                platform,
                                MODE_INTERACTIVE,
                                dockerimage,
                                {},
                                MAX_JOBS,
                                "");
    while(true) {
        auto response = m_context.gql_manager.wait_for_response(msg_id);
        nlohmann::json data_msg = response.second;
        if(data_msg["type"] == "error") {
            out<<"error: likely an invalid query..."<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["errors"] != nullptr) {
            out<<"error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            return;
        } else {
            out<<"bringing up the requested ssh container, this may take a while... (use ctrl-c to cancel) "<<std::endl;
            break;
        }
    }

    msg_id = m_context.gql_manager.subscribe_to_data_stream();
    while(true) {
        auto response = m_context.gql_manager.wait_for_response(msg_id);
        nlohmann::json data_msg = response.second;
        PLOGV << "session start response: " << data_msg.dump(4);
        //out<<data_msg.dump(4)<<std::endl;
        if(data_msg["type"] == "error") {
            out<<"got error in the data stream (abnormal query?)..."<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["errors"] != nullptr) {
            out<<"error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            return;
        } else 
        if(data_msg["payload"]["data"] != nullptr) {
            auto msg = nlohmann::json::parse(data_msg["payload"]["data"]["subsData"]["message"].get<std::string>());

            out << "opening ssh tunnel... ";
            auto tunnel_ret = m_context.ssh.start_ssh_tunnel(name,
                                                                msg["host"].get<std::string>(),
                                                                msg["port"].get<int>());   
            if(tunnel_ret.status) {
                out << "done." << std::endl;
                out << "container is ready, use the following to ssh:" << std::endl;
                out << "\tcommand:\tssh root@localhost -p" << tunnel_ret.local_port << std::endl;
                out << "\tpassword:\t" << msg["password"].get<std::string>() << std::endl;            
                out << "note: stopping this session will terminate the tunnel and interactive container." << std::endl;
            } else {
                out << "failed." << std::endl;
            }
        
            break;
        }  
        if(response.first) {
            out<<"got error in the data stream..."<<std::endl;
            break;
        }
    }
}

void
session_commands::session_stop(std::ostream& out, const std::string& name)
{
    out << "terminating the ssh tunnel... ";
    m_context.ssh.stop_ssh_tunnel(name);
    out << "done" << std::endl;
    
    int msg_id = m_context.gql_manager.session_stop(name);
    while(true) {
        auto response = m_context.gql_manager.wait_for_response(msg_id);
        nlohmann::json data_msg = response.second;
        PLOGV << "session stop response: " << data_msg.dump(4);
        if(data_msg["type"] == "error") {
            out << "error: likely an invalid query..." << std::endl;
            return;
        } else 
        if(data_msg["payload"]["errors"] != nullptr) {
            out << "error: " << data_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
            return;
        } else {
            out << "session '" << data_msg["payload"]["data"]["sessionUpdate"]["name"].get<std::string>() << "' is canceled." << std::endl;
            break;
        }
    }
}

void
session_commands::session_status(std::ostream& out, const std::string& name)
{
    int msg_id = m_context.gql_manager.session_status(name);
    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json data_msg = response.second;
    PLOGV << "session status response: " << data_msg.dump(4);
    if(data_msg["payload"]["data"] != nullptr) {
        out << "session state: " << data_msg["payload"]["data"]["sessionStatus"]["state"].get<std::string>() << std::endl; 
        for (auto& s : data_msg["payload"]["data"]["sessionStatus"]["jobs"]) {
            out << "  #" << s["id"].get<int>() 
                << "\t dataset: " << s["dataset"].get<std::string>() 
                << "\t state: " << s["state"].get<std::string>() << std::endl;
        }
    } else 
    if(data_msg["payload"]["errors"] != nullptr) {
        out << "error: " << data_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
    }
}

std::shared_ptr<cli::Command> 
session_commands::create_session_cmd()
{
    m_session_cmd = create_cmd_helper(
        CMD_SESSION_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_SESSION_NAME, CMD_SESSION_HELP);
            options.add_options()
                ("command", CMD_SESSION_PARAMDESC[0], cxxopts::value<std::string>())
                ("mode", CMD_SESSION_PARAMDESC[1], cxxopts::value<std::string>())
                ("p, platform", CMD_SESSION_PARAMDESC[2], cxxopts::value<std::string>())
                ("d, docker-image", CMD_SESSION_PARAMDESC[4], cxxopts::value<std::string>())
                ("n, name", CMD_SESSION_PARAMDESC[3], cxxopts::value<std::string>())
                ("r, run-script", CMD_SESSION_PARAMDESC[4], cxxopts::value<std::string>())
                ("i, input-datasets", CMD_SESSION_PARAMDESC[5], cxxopts::value<std::string>())
                ("j, jobs", CMD_SESSION_PARAMDESC[6], cxxopts::value<int>()->default_value("1"));

            options.parse_positional({"command", "mode"});

            try {
                auto result = options.parse(argc, argv);
                

                if(result.count("command") != 1) {
                    out << CMD_SESSION_NAME << ": 'command' (either 'start', 'stop' or 'status') "
                        << "is a mandatory argument." << std::endl;
                    return;
                }
                auto command = result["command"].as<std::string>();
                std::string mode = "";
                std::string platform = "";
                std::string dockerimage = "";
                std::string script = "";
                std::vector<std::string> datasets;
                int max_jobs = 1;
                if(command == "start") {
                    if(result.count("mode") != 1) {
                        out << CMD_SESSION_NAME << ": 'mode' (either '" << MODE_INTERACTIVE << "' or '" << MODE_BATCH << "') "
                            << "is a mandatory argument." << std::endl;
                        return;
                    }
                    mode = result["mode"].as<std::string>();
                    if(mode != MODE_INTERACTIVE && mode != MODE_BATCH) {
                        out << CMD_SESSION_NAME << ": unsupported session mode, "
                            << "supported modes are: '" << MODE_INTERACTIVE << "', '" << MODE_BATCH << "'." << std::endl;
                        return;
                    }
                    if(result.count("platform") != 1) {
                        out << CMD_SESSION_NAME << ": '-p|--platform' is a mandatory argument for starting a session." << std::endl;
                        return;
                    }
                    platform = result["platform"].as<std::string>();
                    if(result.count("docker-image") != 1) {
                        out << CMD_SESSION_NAME << ": '-d|--docker-image' is a mandatory argument for starting a session." << std::endl;
                        return;
                    }
                    dockerimage = result["docker-image"].as<std::string>();

                    if(mode == MODE_BATCH) {
                        if(result.count("run-script") != 1) {
                            out << CMD_SESSION_NAME << ": '-r|--run-script' is a mandatory argument for starting a batch session." << std::endl;
                            return;
                        }
                        script = result["run-script"].as<std::string>();
                        if(result.count("input-datasets") != 1) {
                            out << CMD_SESSION_NAME << ": '-i|--input-datasets' is a mandatory argument for starting a batch session." << std::endl;
                            return;
                        }
                        std::string ds = result["input-datasets"].as<std::string>();
                        ds.erase(std::remove(ds.begin(), ds.end(), ' '), ds.end());
        
                        std::regex reg("[,]+");
                        std::sregex_token_iterator begin(ds.begin(), ds.end(), reg, -1);
                        std::sregex_token_iterator end;
                        datasets.insert(datasets.begin(), begin, end);
                        if(datasets.empty()) {
                            out << CMD_SESSION_NAME << ": the argument for --input-datasets must be in ds1,ds2,...dsn format."<< std::endl;
                            return;
                        }

                        if(result.count("jobs") == 1) {
                            max_jobs = result["jobs"].as<int>();
                        }
                    }                    
                }

                std::string name = "";
                if(result.count("name")) {
                    name = result["name"].as<std::string>();
                } else {
                    out << CMD_SESSION_NAME << ": '-n|--name' is a mandatory argument." << std::endl;                    
                    return;
                }

                if(command == "start") {
                    if(mode == MODE_BATCH) {
                        session_start_batch(out, name, dockerimage, platform, script, max_jobs, datasets);
                    } else {
                        session_start_interactive(out, name, dockerimage, platform);
                    }
                } else 
                if(command == "stop") {
                    session_stop(out, name);
                } else 
                if(command == "status") {
                    session_status(out, name);
                } else {
                    out << CMD_SESSION_NAME << ": unsupported session command, "
                        << "supported commands are: 'start', 'stop', 'status'." << std::endl;
                    return;
                }
            } catch (std::exception& e) {
                out << CMD_SESSION_NAME << ": " << e.what() << std::endl;
                print_session_usage(out);
                return;
            }        
        },
        CMD_SESSION_HELP,
        CMD_SESSION_PARAMDESC
    );
    return m_session_cmd;
}

} // namespace metriffic
