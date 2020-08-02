#ifndef SSH_MANAGER_HPP
#define SSH_MANAGER_HPP

#include <experimental/filesystem>
#include <libssh2.h>
#include <netinet/in.h>
#include <thread>
#include <memory>
#include <map>
#include <list>

namespace metriffic
{

class ssh_manager
{ 
public:
    struct ssh_tunnel_ret 
    {
        ssh_tunnel_ret(bool s = false, unsigned int p = -1, const std::string& host = "")
         : status(s), local_port(p), dest_host(host)
        {}
        bool status;
        unsigned int local_port;
        const std::string dest_host;
    };

private:   
    class ssh_tunnel
    {
    public:
        struct one_session {
            one_session();
            LIBSSH2_SESSION* session;
            LIBSSH2_CHANNEL* channel;
            int sock;
            int forwardsock;
            std::thread io_thread;
        };
        ssh_tunnel(const std::string& username,
                   const std::string& password,
                   const std::string& local_host,
                   std::pair<unsigned int, unsigned int> local_port_range,
                   const std::string& bastion_host,
                   const unsigned int bastion_port,
                   const std::string& dest_host,
                   const unsigned int dest_port);
        ~ssh_tunnel();
        ssh_tunnel_ret start();
        void stop();

    private:
        bool run();
        bool setup_listening_socket();
        bool establish_connection_to_bastion(one_session& os);
        bool service_io(one_session& os);
        int connect_to_bastion();

    private:
        std::thread m_thread;

        bool m_should_stop;
        std::string m_username;
        std::string m_password;
        std::string m_local_host;
        std::pair<unsigned int, unsigned int> m_local_port_range;
        std::string m_bastion_host;
        unsigned int m_bastion_port;
        std::string m_dest_host;
        unsigned int m_dest_port;
        unsigned int m_local_port;
        int m_listen_sock;
        std::list<one_session> m_all_sessions;
    };

public:
    ssh_manager();
    ~ssh_manager();

    ssh_tunnel_ret start_ssh_tunnel(const std::string& session_name,
                                    const std::string& desthost,
                                    const unsigned int destport);  
    void stop_ssh_tunnel(const std::string& session_name);  
    
    ssh_tunnel_ret start_rsync_tunnel(const std::string& username)
    {
        return start_ssh_tunnel("rsync." + username, 
                                RSYNC_SERVER_HOSTNAME, 
                                RSYNC_SERVER_PORT);
    }
    void stop_rsync_tunnel(const std::string& username)
    {
        return stop_ssh_tunnel("rsync." + username);
    }

private:

    const std::string  LOCAL_SSH_HOSTNAME   = "127.0.0.1";
    const unsigned int LOCAL_SSH_PORT_START   = 2000;
    const std::string  BASTION_SSH_HOSTNAME = "metriffic.com";
    const unsigned int BASTION_SSH_PORT = 2222;

    const std::string  BASTION_SSH_USERNAME = "ssh_user";
    const std::string  BASTION_SSH_PASSWORD = "ssh_user";

    const std::string  RSYNC_SERVER_HOSTNAME = "metriffic";
    const unsigned int RSYNC_SERVER_PORT = 7000;

    std::map<std::string, std::unique_ptr<ssh_tunnel>> m_session_tunnels;
    bool m_should_stop = false;
};

} // namespace metriffic

#endif //SSH_MANAGER_HPP
