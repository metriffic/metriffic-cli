#ifndef SSH_MANAGER_HPP
#define SSH_MANAGER_HPP

#include <experimental/filesystem>
#include <libssh2.h>
#include <netinet/in.h>
#include <thread>
#include <memory>
#include <map>

namespace metriffic
{

class ssh_manager
{    
    class ssh_tunnel
    {
    public:
        ssh_tunnel(const std::string& username,
               const std::string& password,
               const std::string& desthost,
               const unsigned int destport);
        ~ssh_tunnel();
        std::pair<bool, unsigned int> init();
        void run();
        void stop();

    private:
        bool run_service(int forwardsock, LIBSSH2_CHANNEL* channel);

    private:
        std::thread m_thread;

        bool m_should_stop;
        std::string m_username;
        std::string m_password;
        std::string m_desthost;
        unsigned int m_destport;

        struct sockaddr_in m_sin;
        LIBSSH2_SESSION* m_session;
        LIBSSH2_CHANNEL* m_channel;
        int m_sockopt;
        int m_sock;
        int m_listensock;
        int m_forwardsock;
    };

public:
    ssh_manager();
    ~ssh_manager();

    std::pair<bool, unsigned int> start_ssh_tunnel(const std::string& session_name,
                                                   const std::string& username,
                                                   const std::string& password,
                                                   const std::string& desthost,
                                                   const unsigned int destport);  
    void stop_ssh_tunnel(const std::string& session_name);  

private:
    std::map<std::string, std::unique_ptr<ssh_tunnel>> m_session_tunnels;
    bool m_should_stop = false;
};

} // namespace metriffic

#endif //SSH_MANAGER_HPP
