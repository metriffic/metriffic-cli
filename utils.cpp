#include "utils.hpp"
#include <regex>
#include <iostream>
#include <termios.h>
#include <unistd.h>

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
        //("^([0-9a-zA-Z]([-.\\w]*[0-9a-zA-Z])*@([0-9a-zA-Z][-\\w]*[0-9a-zA-Z]\\.)+[a-zA-Z]{2,9})$");
    // try to match the string with the regular expression
    return std::regex_match(email, pattern);
}