#include <regex>
#include <cli/cli.h>
#include <cxxopts.hpp>
#include <plog/Log.h>

#include "session_commands.hpp"
#include "context.hpp"

namespace metriffic
{

void
show_progress(std::ostream& out, const std::string& what, float progress)
{
    if(progress < 0.0) progress = 0.0;
    if(progress > 1.0) progress = 1.0;

    constexpr int BARWIDTH = 30;

    std::cout << "\r\t" << what << " [";
    int pos = BARWIDTH * progress;
    for (int i = 0; i < BARWIDTH; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) out << ">";
        else out << " ";
    }
    out << "] " << int(progress * 100.0) << " %";
    out.flush();
}

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
        if(data_msg["payload"].contains("errors")) {
            out<<"error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            return;
        } else {
            break;
        }
    }
}

void
session_commands::session_start_interactive(std::ostream& out, 
                                            const std::string& name, 
                                            const std::string& dockerimage, 
                                            const std::string& platform)
{
    const int MAX_JOBS = 1;    
    int sbs_msg_id = m_context.gql_manager.subscribe_to_data_stream();
    int msg_id = m_context.gql_manager.session_start(
                                name,
                                platform,
                                MODE_INTERACTIVE,
                                dockerimage,
                                {},
                                MAX_JOBS,
                                "");
    bool in_progress = false;

    while(true) {
        auto response = m_context.gql_manager.wait_for_response({msg_id, sbs_msg_id});

        if(response.first) {
            if(in_progress) {
                out << std::endl;
            }
            out<< "interrupted..." << std::endl;
            break;
        }

        for(const auto& data_msg : response.second ) {
            PLOGV << "session start response: " << data_msg.dump(4);

            if(!data_msg.contains("payload") || data_msg["payload"] == nullptr) {
                continue;
            }
            if(data_msg["type"] == "error") {
                out << "datastream error (abnormal query?)..." << std::endl;
                return;
            } else 
            if(data_msg["payload"].contains("errors")) {
                out << "error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
                return;
            }


            if(data_msg["id"] == msg_id) {
                out << "bringing up the requested ssh container, this may take a while..." << std::endl;
                out << "note: ctrl-c will cancel the request..." << std::endl << std::endl;
            } else
            if(data_msg["id"] == sbs_msg_id) {
                if(data_msg["payload"].contains("data")) {
                    auto msg = nlohmann::json::parse(data_msg["payload"]["data"]["subsData"]["message"].get<std::string>());

                    if(msg.contains("type")) {
                        if(msg["type"] == "pull_data") {
                            auto data = nlohmann::json::parse(msg["data"].get<std::string>());

                            if(data["status"] == "Downloading") {
                                if(in_progress == false) {
                                    out << "loading docker image. " << std::endl;
                                }
                                in_progress = true;
                                float done = data["progressDetail"]["current"].get<int>();
                                float total = data["progressDetail"]["total"].get<int>();
                                show_progress(out, "downloading:", done/total);
                            }
                            if(data["status"] == "Download complete") {
                                show_progress(out, "downloading:", 1.0);
                                out << std::endl; 
                            }

                            if(data["status"] == "Extracting") {
                                if(in_progress == false) {
                                    out << "loading docker image. " << std::endl;
                                }
                                float done = data["progressDetail"]["current"].get<int>();
                                float total = data["progressDetail"]["total"].get<int>();
                                show_progress(out, "extracting: ", done/total);
                            }
                            if(data["status"] == "Pull complete") {
                                show_progress(out, "extracting: ", 1.0);
                                out << std::endl; 
                            }

                        } else 
                        if(msg["type"] == "pull_success") {
                            out << "docker image is ready. " << std::endl;                            
                        } else 
                        if(msg["type"] == "pull_error") {
                            out << "got error while pulling the image: " << msg["error"].get<std::string>() << std::endl;                            
                        } else
                        if(msg["type"] == "exec_success") {
                            out << "container is up. " << std::endl;
                            out << "opening ssh tunnel... ";
                            auto data = msg["data"];
                            auto tunnel_ret = m_context.ssh.start_ssh_tunnel(
                                                            name,
                                                            data["host"].get<std::string>(),
                                                            data["port"].get<int>());   
                            if(tunnel_ret.status) {
                                out << "done." << std::endl;
                                out << "container is ready, use the following to ssh:" << std::endl;
                                out << "\tcommand:\tssh root@localhost -p" << tunnel_ret.local_port << std::endl;
                                out << "\tpassword:\t" << data["password"].get<std::string>() << std::endl;            
                                out << "note: stopping this session will terminate the tunnel and interactive container." << std::endl;
                            } else {
                                out << "failed." << std::endl;
                            }     
                            return;           
                        } else
                        if(msg["type"] == "start_error") {
                            // tbd: the detailed error json is here: msg["error"].get<std::string>()
                            out << "error: failed to start the container due to docker related issue on the board..." << std::endl;
                            return;    
                        }
                    }
                } else {
                    out << "missing diagnostics data (communcation error?)..." << std::endl;
                }
            }
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
        if(data_msg["payload"].contains("errors")) {
            out << "error: " << data_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
            return;
        } else {
            out << "session '" << data_msg["payload"]["data"]["sessionUpdate"]["name"].get<std::string>() << "' is canceled." << std::endl;
            break;
        }
    }
}

void
session_commands::session_save(std::ostream& out, const std::string& name, 
                               const std::string& dockerimage, const std::string& comment)
{
    int sbs_msg_id = m_context.gql_manager.subscribe_to_data_stream();
    int msg_id = m_context.gql_manager.session_save(name, dockerimage, comment);
    bool in_progress = false;
   
    while(true) {
        auto response = m_context.gql_manager.wait_for_response({msg_id, sbs_msg_id});
        if(response.first) {
            if(in_progress) {
                out << std::endl;
            }
            out << "interrupted, the docker image will be saved in the background..." << std::endl;
            break;
        }

        for(const auto& data_msg : response.second ) {
            PLOGV << "session save response: " << data_msg.dump(4);

            if(!data_msg.contains("payload") || data_msg["payload"] == nullptr) {
                continue;
            }
            if(data_msg["type"] == "error") {
                out << "datastream error (abnormal query?)..." << std::endl;
                return;
            } else 
            if(data_msg["payload"].contains("errors")) {
                out << "error: "<<data_msg["payload"]["errors"][0]["message"].get<std::string>() << std::endl;
                return;
            }

            if(data_msg["id"] == sbs_msg_id) {
                if(data_msg["payload"].contains("data")) {

                    auto msg = nlohmann::json::parse(data_msg["payload"]["data"]["subsData"]["message"].get<std::string>());
                    if(msg["type"] == "push_data") {
                        auto data = nlohmann::json::parse(msg["data"].get<std::string>());
                        if(data["status"] == "Pushing") {
                            if(in_progress == false) {
                                out << "preparing for save..." << std::endl;
                                in_progress = true;
                            }                            
                            float done = data["progressDetail"]["current"].get<int>();
                            float total = data["progressDetail"]["total"].get<int>();
                            show_progress(out, "saving:", done/total);
                        }
                        if(data["status"] == "Pushed") {
                            show_progress(out, "saving:", 1.0);
                            out << std::endl; 
                        }

                    } else 
                    if(msg["type"] == "push_success") {
                        out << "docker image is saved." << std::endl;
                    } else 
                    if(msg["type"] == "register_success") {
                        out << "docker image is registered in the database." << std::endl;
                        return;    
                    } else
                    if(msg["type"] == "commit_error") {
                        // TBD: more details are msg["error"].get<std::string>() 
                        out << "error: failed to commit the docker image." << std::endl;
                        return;
                    } else
                    if(msg["type"] == "push_error") {
                        // TBD: more details are msg["error"].get<std::string>() 
                        out << "error: failed to push the docker image." << std::endl;
                        return;
                    } else
                    if(msg["type"] == "register_error") {
                        // TBD: more details are msg["error"].get<std::string>() 
                        out << "error: failed to register the docker image." << std::endl;
                        return;
                    } else
                    if(msg["type"] == "save_error") {
                        // TBD: more details are msg["error"].get<std::string>() 
                        out << "error: failed to save the docker image." << std::endl;
                        return;
                    }

                } else {
                    out << "missing diagnostics data (communication error?)..." << std::endl;
                }
            }
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
    if(data_msg["payload"].contains("errors")) {
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
                std::string comment = "";
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
                if(command == "save") {
                    if(result.count("docker-image") != 1) {
                        out << CMD_SESSION_NAME << ": '-d|--docker-image' is a mandatory argument for saving a session." << std::endl;
                        return;
                    }
                    dockerimage = result["docker-image"].as<std::string>();
                    if(result.count("comment")) {
                        comment = result["docker-image"].as<std::string>();
                    }
                    session_save(out, name, dockerimage, comment);
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
