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

    std::shared_ptr<cli::Command> create_register_cmd();
    std::shared_ptr<cli::Command> create_login_cmd();
    std::shared_ptr<cli::Command> create_logout_cmd();

private: 
    Context& m_context;
    std::shared_ptr<cli::Command> m_register_cmd;
    std::shared_ptr<cli::Command> m_login_cmd;
    std::shared_ptr<cli::Command> m_logout_cmd;
};

} // namespace metriffic

#endif //AUTHENTICATION_COMMANDS_HPP
