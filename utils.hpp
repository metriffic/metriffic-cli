#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <cli/cli.h>


int capture_char();

std::string capture_password();

bool validate_email(const std::string& email);

template<typename F>
std::shared_ptr<cli::Command> 
create_cmd_helper(const std::string& name,
                  F f,
                  const std::string& help,
                  const std::vector<std::string>& par_desc) 
{
    return std::make_shared<cli::ShellLikeFunctionCommand<F>>(name, f, help, par_desc); 
}

#endif