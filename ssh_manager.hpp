#ifndef SSH_MANAGER_HPP
#define SSH_MANAGER_HPP

#include <experimental/filesystem>

namespace metriffic
{

class ssh_manager
{    
public:
    ssh_manager();

    bool start_ssh_tunnel(const std::string& username,
                         const std::string& password,
                         const std::string& desthost,
                         const unsigned int destport);                         
};

} // namespace metriffic

#endif //SSH_MANAGER_HPP
