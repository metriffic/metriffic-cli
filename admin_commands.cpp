#include "admin_commands.hpp"
#include "utils.hpp"

#include "utils.hpp"
#include <cxxopts.hpp>
#include <termcolor/termcolor.hpp>
#include <plog/Log.h>
#include <regex>
#include <algorithm>

                        
namespace metriffic
{

namespace tc = termcolor;

admin_commands::admin_commands(Context& c)
 : m_context(c)
{}

void 
admin_commands::dump_diagnostics(std::ostream& out, const nlohmann::json& msg)
{
    out << std::endl << "printing platform diagnostics:" << std::endl;
    for (const auto& pel : msg["platforms"].items()) {
        const auto& p = pel.value();
        out << "    platform: " << tc::bold << tc::underline <<  tc::blue << p["name"].get<std::string>() << tc::reset << std::endl;
        for (const auto& bel : p["boards"].items()) {
            const auto& b = bel.value();
            out << "        hostname: " << tc::bold << b["hostname"].get<std::string>() << tc::reset << std::endl;
            out << "             status: " << tc::bold << (b["alive"] ? "alive" : "dead") << tc::reset << std::endl;
            out << "             ping: " << tc::bold << b["ping"].get<std::string>() << tc::reset << std::endl;
            out << "             in use: " << tc::bold << (b["used"] ? "yes" : "no") << tc::reset << std::endl;
        }
    }
    
    out << std::endl << "printing session diagnostics:" << std::endl;    
    for (const auto& pel : msg["sessions"].items()) {
        const auto& p = pel.value();
        out << "    platform: " << tc::bold << tc::underline <<  tc::blue << p["name"].get<std::string>() << tc::reset << std::endl;    
        out << "        submitted sessions:" << tc::bold << p["sessions"].size() << tc::reset << std::endl;
        for (const auto& sel : p["sessions"].items()) {
            const auto& s = sel.value();
            out << "            session: " << tc::bold << tc::underline << s["name"].get<std::string>() << tc::reset << std::endl;
            out << "                 user:      " << tc::bold << s["user"].get<std::string>() << tc::reset << std::endl;
            out << "                 total:     " << tc::bold << s["total_jobs"].get<int>() << tc::reset << std::endl;
            out << "                 running:   " << tc::bold << s["running_jobs"].get<int>() << tc::reset << std::endl;
            out << "                 remaining: " << tc::bold << s["remaining_jobs"].get<int>() << tc::reset << std::endl;
        }
        out << "        running jobs:" << tc::bold << p["running_jobs"].size() << tc::reset << std::endl;
        for (const auto& rel : p["running_jobs"].items()) {
            const auto& r = rel.value();
            out << "            job: " << tc::bold << tc::underline << r["name"].get<std::string>() << tc::reset << std::endl;
            out << "                 session:   " << tc::bold << r["session"].get<std::string>() << tc::reset << std::endl;
            out << "                 type:      " << tc::bold << r["type"].get<std::string>() << tc::reset << std::endl;
            out << "                 start:     " << tc::bold << r["start"].get<std::string>() << tc::reset << std::endl;
            out << "                 container: " << tc::bold << r["container"].get<std::string>() << tc::reset << std::endl;
            out << "                 hostname:  " << tc::bold << r["board"].get<std::string>() << tc::reset << std::endl;
        }
    }

}

void
admin_commands::admin_diagnostics(std::ostream& out)
{
    int sbs_msg_id = m_context.gql_manager.subscribe_to_data_stream();
    int msg_id = m_context.gql_manager.admin_diagnostics();

    while(true) {
        auto response = m_context.gql_manager.wait_for_response({msg_id, sbs_msg_id});
        if(response.first) {
            out << "interrupted..." << std::endl;
            break;
        }

        for(const auto& data_msg : response.second ) {
            PLOGV << "admin diagnostics response: " << data_msg.dump(4);

            if(!data_msg.contains("payload")) {
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
                    dump_diagnostics(out, msg);
                    return;
                } else {
                    out << "missing diagnostics data (communcation error?)..." << std::endl;
                }
            }
        }
    }
}


std::shared_ptr<cli::Command> 
admin_commands::create_admin_cmd()
{
    return create_cmd_helper(
        CMD_ADMIN_NAME,
        [this](std::ostream& out, int argc, char** argv){ 

            cxxopts::Options options(CMD_ADMIN_NAME, CMD_ADMIN_HELP);
            options.add_options()
                ("subcommand", CMD_ADMIN_PARAMDESC[0], cxxopts::value<std::string>());

            options.parse_positional({"subcommand"});

            try {
                auto result = options.parse(argc, argv);
                

                if(result.count("subcommand") != 1) {
                    out << CMD_ADMIN_NAME << ": 'subcommand' (currently only '"<<CMD_SUB_DIAGNOSTICS<<"') "
                        << "is a mandatory argument." << std::endl;
                    return;
                }
                auto subcommand = result["subcommand"].as<std::string>();

                if(subcommand == CMD_SUB_DIAGNOSTICS) {
                    admin_register(out);
                } else 
                if(subcommand == CMD_SUB_REGISTER) {
                    admin_register(out);
                } else {
                    out << CMD_ADMIN_NAME << ": unsupported subcommand type, "
                        << "the only supported subcommand for now is '"<<CMD_SUB_DIAGNOSTICS<<"'." << std::endl;
                    return;
                }
            } catch (std::exception& e) {
                out << CMD_ADMIN_NAME << ": " << e.what() << std::endl;
                return;
            }        
        },
        CMD_ADMIN_HELP,
        CMD_ADMIN_PARAMDESC
    );
}

void admin_commands::admin_register(std::ostream& out)
{
    m_context.session.disable_input();                
    std::string username, email;
    std::cout << "enter login: ";
    std::cin >> username; 
    std::cout << "enter email: ";
    std::cin >> email; 
    if(!validate_email(email)) {
        std::cout << "not a valid email address. Failed to register, try again..."<<std::endl;
        m_context.session.enable_input();
        return;
    }
    std::cout << "enter password: ";   
    // read the spurious return-char at the end 
    getchar();
    std::string password = capture_password();
    std::cout << "re-enter password: ";            
    std::string repassword = capture_password();
    if(password != repassword) {
        std::cout << "passwords don't match. Failed to register, try again..."<<std::endl;
        m_context.session.enable_input();
        return;
    }
    int msg_id = m_context.gql_manager.registr(username, email, password, repassword);                            
    m_context.session.enable_input();

    auto response = m_context.gql_manager.wait_for_response(msg_id);
    nlohmann::json& register_msg  = response.second;
    if(register_msg["payload"]["data"] != nullptr) {
        std::cout<<"registration is successful!"<<std::endl;
        auto data = register_msg["payload"]["data"]["register"];
        m_context.logged_in(data["username"], data["token"]);
        initialize_new_user(username);
    } else 
    if(register_msg["payload"].contains("errors") ) {
        std::cout<<"registration failed: "<<register_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
        m_context.logged_out();
    }
}

void 
admin_commands::initialize_new_user(const std::string& username)
{
    m_context.settings.create_user(username);
}

} // namespace metriffic
