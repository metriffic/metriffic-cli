#include "utils.hpp"
#include <regex>
#include <iostream>
#include <termios.h>
#include <unistd.h>

bool validate_email(const std::string& email) 
{
    // define a regular expression
    const std::regex pattern
        ("(\\w+)(\\.|_)?(\\w*)@(\\w+)(\\.(\\w+))+");
        //("^([0-9a-zA-Z]([-.\\w]*[0-9a-zA-Z])*@([0-9a-zA-Z][-\\w]*[0-9a-zA-Z]\\.)+[a-zA-Z]{2,9})$");
    // try to match the string with the regular expression
    return std::regex_match(email, pattern);
}