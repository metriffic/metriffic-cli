#ifndef AUTHENTICATION_COMMANDS_HPP
#define AUTHENTICATION_COMMANDS_HPP

#include <string>
#include <memory>
#include <cli/cli.h>

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
    const std::string CMD_LOGIN_NAME = "login";
    const std::string CMD_LOGIN_HELP = "log in to metriffic service";
    const std::vector<std::string> CMD_LOGIN_PARAMDESC = {};
    const std::string CMD_LOGOUT_NAME = "login";
    const std::string CMD_LOGOUT_HELP = "Log out from the service";    
    const std::vector<std::string> CMD_LOGOUT_PARAMDESC = {};
    
private: 
    Context& m_context;
};

} // namespace metriffic

#endif //AUTHENTICATION_COMMANDS_HPP
