#include <regex>
#include <fstream>
#include <experimental/filesystem>

#include "authentication_commands.hpp"

namespace metriffic
{



template<typename F>
std::shared_ptr<cli::Command> 
create_cmd_helper(const std::string& name,
                  F f,
                  const std::string& help) 
{
    return std::make_shared<cli::ShellLikeFunctionCommand<F>>(name, f, help); 
}

int capture_char() {
    int ch;
    struct termios t_old, t_new;
    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    return ch;
}

std::string capture_password()
{
    unsigned char ch;
    std::string password;
    while((ch = capture_char()) != 10) {
        if(ch == 127) {
            if(password.length() != 0) {
                std::cout << "\b \b";
                password.resize(password.length()-1);
            }
        } else {
            password += ch;
            std::cout << '*';
        }
    }
    std::cout << std::endl;
    return password;
}

bool validate_email(const std::string& email) 
{
    // define a regular expression
    const std::regex pattern
        ("(\\w+)(\\.|_)?(\\w*)@(\\w+)(\\.(\\w+))+");
        ("^([0-9a-zA-Z]([-.\\w]*[0-9a-zA-Z])*@([0-9a-zA-Z][-\\w]*[0-9a-zA-Z]\\.)+[a-zA-Z]{2,9})$");
    // try to match the string with the regular expression
    return std::regex_match(email, pattern);
}

authentication_commands::authentication_commands(Context& c)
 : m_context(c)
{}

void 
authentication_commands::initialize_new_user(const std::string& username)
{
    m_context.settings.create_user(username);
}

std::shared_ptr<cli::Command> 
authentication_commands::create_register_cmd()
{
    m_register_cmd = create_cmd_helper(
        "register",
        [this](std::ostream& out, int, char**){ 
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
        },
        "log in to metriffic service"
    );
    return m_register_cmd;
}

std::shared_ptr<cli::Command> 
authentication_commands::create_login_cmd()
{
    m_login_cmd = create_cmd_helper(
        "login",
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
                m_context.logged_in(data["username"], data["token"]);
            } else 
            if(login_msg["payload"].contains("errors") ) {
                std::cout<<"login failed: "<<login_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
                m_context.logged_out();
            }
        },
        "log in to metriffic service"
    );
    return m_login_cmd;
}

std::shared_ptr<cli::Command> 
authentication_commands::create_logout_cmd()
{
    m_logout_cmd = create_cmd_helper(
        "logout",
        [this](std::ostream& out, int, char**){
            int msg_id = m_context.gql_manager.logout();
            auto response = m_context.gql_manager.wait_for_response(msg_id);
            nlohmann::json& logout_msg  = response.second;

            if(logout_msg["payload"]["data"] != nullptr) {
                std::cout<<"User "<<logout_msg["payload"]["data"]["logout"].get<std::string>()<<" has successfully logged out..."<<std::endl;
                m_context.logged_out();
            } else 
            if(logout_msg["payload"].contains("errors") ) {
                std::cout<<"Failed to log out: "<<logout_msg["payload"]["errors"][0]["message"].get<std::string>()<<std::endl;
            }
        },
        "Log out from the service"
    );
    return m_logout_cmd;

}

} // namespace metriffic
