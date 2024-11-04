#ifndef AUTHENTICATION_COMMANDS_HPP
#define AUTHENTICATION_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>
#include <openssl/evp.h>

#include "context.hpp"

namespace metriffic
{

class authentication_commands
{    
public:
    authentication_commands(Context& c);

    std::shared_ptr<cli::Command> create_login_cmd();
    std::shared_ptr<cli::Command> create_logout_cmd();
private:
    std::string generate_token(const std::string& username);
    std::vector<uint8_t> sign_with_private_key(const std::string& data, const std::string& private_key_path);
private:
    const std::string CMD_LOGIN_NAME = "login";
    const std::string CMD_LOGIN_HELP = "log in to metriffic service";
    const std::vector<std::string> CMD_LOGIN_PARAMDESC = {{"<username>: mandatory argument"}};
    const std::string CMD_LOGOUT_NAME = "logout";
    const std::string CMD_LOGOUT_HELP = "Log out from the service";    
    const std::vector<std::string> CMD_LOGOUT_PARAMDESC = {};
    
private: 
    Context& m_context;
};

} // namespace metriffic

#endif //AUTHENTICATION_COMMANDS_HPP
