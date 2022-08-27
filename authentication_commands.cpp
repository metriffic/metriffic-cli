#include <fstream>
#include <filesystem>

#include "authentication_commands.hpp"
#include "utils.hpp"

namespace metriffic
{

authentication_commands::authentication_commands(Context& c)
 : m_context(c)
{}

std::shared_ptr<cli::Command> 
authentication_commands::create_login_cmd()
{
    return create_cmd_helper(
        CMD_LOGIN_NAME,
        [this](std::ostream& out, int, char**){ 
            m_context.session.disable_input();                
            std::string username;
            std::cout << "enter login: ";
            std::cin >> username; 
            std::cout << "enter password: ";            
            // read the spurious return-char at the end 
            getchar();
            std::string password = capture_password();

            int msg_id = m_context.gql_manager.login(username, password);
            m_context.session.enable_input();

            auto response = m_context.gql_manager.wait_for_response(msg_id);
            nlohmann::json& login_msg  = response.second;
            if(login_msg["payload"]["data"] != nullptr) {
                std::cout<<"login successful!"<<std::endl;
                auto& data = login_msg["payload"]["data"]["login"];
                const auto& username = data["username"];
                const auto& token = data["token"];
                m_context.logged_in(username, token);
                if(!m_context.settings.user_config_exists(username)) {
                    m_context.settings.create_user(username);
                }
                m_context.settings.set_active_user(m_context.username, token);
            } else 
            if(login_msg["payload"].contains("errors") ) {
                std::cout<<"login failed: "<<login_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
                m_context.logged_out();
            }
        },
        CMD_LOGIN_HELP,
        CMD_LOGIN_PARAMDESC
    );
}

std::shared_ptr<cli::Command> 
authentication_commands::create_logout_cmd()
{
    return create_cmd_helper(
        CMD_LOGOUT_NAME,
        [this](std::ostream& out, int, char**){
            int msg_id = m_context.gql_manager.logout();
            auto response = m_context.gql_manager.wait_for_response(msg_id);
            nlohmann::json& logout_msg  = response.second;

            if(logout_msg["payload"]["data"] != nullptr) {
                std::cout<<"User "<<logout_msg["payload"]["data"]["logout"].get<std::string>()<<" has successfully logged out..."<<std::endl;
                m_context.logged_out();
                m_context.settings.clear_active_user();
            } else 
            if(logout_msg["payload"].contains("errors") ) {
                std::cout<<"Failed to log out: "<<logout_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            }
        },
        CMD_LOGOUT_HELP,
        CMD_LOGOUT_PARAMDESC
    );

}

} // namespace metriffic
