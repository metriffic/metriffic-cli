#include <nlohmann/json.hpp>
#include <plog/Log.h>
#include <iostream>
#include <fstream>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include <libssh2.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
 
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include "ssh_manager.hpp"

namespace fs = std::filesystem;

namespace metriffic
{

ssh_manager::ssh_tunnel::ssh_tunnel(
               const std::string& username,
               const std::string& bastion_public_key,
               const std::string& bastion_private_key,
               const std::string& local_host,
               std::pair<unsigned int, unsigned int> local_port_range,
               const std::string& bastion_host,
               const unsigned int bastion_port,
               const std::string& dest_host,
               const unsigned int dest_port) 
  : m_should_stop(false),
    m_username(username),
    m_bastion_public_key(bastion_public_key),
    m_bastion_private_key(bastion_private_key),
    m_local_host(local_host),
    m_local_port_range(local_port_range),
    m_bastion_host(bastion_host),
    m_bastion_port(bastion_port),
    m_dest_host(dest_host),
    m_dest_port(dest_port),
    m_local_port(-1),
    m_listen_sock(-1)
{}

ssh_manager::ssh_tunnel::~ssh_tunnel() 
{
}

ssh_manager::ssh_tunnel::one_session::one_session()
  : session(NULL),
    channel(NULL),
    sock(-1),
    forwardsock(-1)
{}

int 
ssh_manager::ssh_tunnel::connect_to_bastion()
{
    int sd = -1;
    struct addrinfo hints = {}, *addrs;
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port = std::to_string(m_bastion_port);

    int err = getaddrinfo(m_bastion_host.c_str(), port.c_str(), &hints, &addrs);
    if (err == 0) {
        for(struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
            sd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (sd == -1) {
                break; 
            }

            if (connect(sd, addr->ai_addr, addr->ai_addrlen) == 0) {
                break;
            }

            close(sd);
            sd = -1;
        }
    } else {
        PLOGE << "[bastion] error: failed to get address to bastion: " << strerror(err);
    }

    freeaddrinfo(addrs);

    return sd;
}

bool
ssh_manager::ssh_tunnel::setup_listening_socket()
{
    struct sockaddr_in serv_addr;

    for (m_local_port = m_local_port_range.first; m_local_port <= m_local_port_range.second; ++m_local_port) {
        m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (m_listen_sock < 0) {
            std::cerr << "[tunnel] error: opening socket." << std::endl;
            continue;
        }
        static int sockopt = 1;
        setsockopt(m_listen_sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(m_local_host.c_str());
        serv_addr.sin_port = htons(m_local_port);


        if(-1 == bind(m_listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
            PLOGE << "[tunnel] trying to bind: " << strerror(errno);
            close(m_listen_sock);
            continue;
        }
        if(-1 == listen(m_listen_sock, 2)) {
            PLOGE << "[tunnel] trying to listen: " << strerror(errno);
            close(m_listen_sock);
            continue;
        }
        PLOGV << "[tunnel] server listening on port " << m_local_port << std::endl;
        return true;
    }
    return false;    
}

ssh_manager::ssh_tunnel_ret
ssh_manager::ssh_tunnel::start()
{
    if(setup_listening_socket() == false ) {
        return ssh_tunnel_ret(false);
    }
    m_thread = std::thread([this]() {
        while( m_should_stop == false ) {
            bool result = run();
            if(result == false) {
                PLOGE << "[tunnel] error: tunnel thread failed";
                break;
            }
        };
    });
    return ssh_tunnel_ret(true, m_local_port, m_dest_host); 
}

void 
ssh_manager::ssh_tunnel::stop()
{
    m_should_stop = true;
    close(m_listen_sock);
    for (auto& os : m_all_sessions) {
        close(os.forwardsock);
        if(os.channel) {
            libssh2_channel_free(os.channel);
        }
        libssh2_session_disconnect(os.session, "");
        libssh2_session_free(os.session);
        close(os.sock); 
        os.io_thread.join();
    }
    m_all_sessions.clear();
    m_thread.join();
}

bool
ssh_manager::ssh_tunnel::establish_connection_to_bastion(one_session& os)
{    
    // Connect to SSH server 
    os.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(os.sock == -1) { 
        PLOGE << "[bastion] error: failed to create a socket!";
        return false;
    }
    os.sock = connect_to_bastion();
    if(os.sock == -1) {
        PLOGE << "[bastion] error: failed to connect to bastion: " << strerror(errno);
        return false;
    }
    // Create a session instance 
    os.session = libssh2_session_init();

    if(!os.session) {
        PLOGE << "[bastion] error: could not initialize SSH session!";
        return false;
    }
    // Start it up. This will trade welcome banners, exchange keys,
    // and setup crypto, compression, and MAC layers
    int rc = libssh2_session_handshake(os.session, os.sock);

    if(rc) {
        PLOGE << "[bastion] error: failed to start up SSH session: " << rc;
        return false;
    }

    if(libssh2_userauth_publickey_fromfile(os.session, 
                                           m_username.c_str(), 
                                           m_bastion_public_key.c_str(), 
                                           m_bastion_private_key.c_str(), 
                                           "")) {
        PLOGE << "[bastion] error: authentication by private key failed!";
        return false;
    }
    PLOGV << "[bastion] authentication  by private key is succeeded!";
    
    return true;
}

bool 
ssh_manager::ssh_tunnel::run()
{    
    one_session os;
    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);

    fd_set allset;
    struct timeval tv;
    tv.tv_sec = 0; 
    tv.tv_usec = 100000;	


    while(m_should_stop == false) {    
        FD_ZERO(&allset);
        FD_SET(m_listen_sock, &allset);
        int max_fd = m_listen_sock;

        int activity = select(max_fd + 1, &allset, nullptr, nullptr, &tv);

        if (activity < 0 && errno != EINTR) {
            PLOGE << "select: " << strerror(errno);
            break;
        }

        if (activity == 0) {
            // Timeout, no activity
            continue;
        }

        if (FD_ISSET(m_listen_sock, &allset)) {
            os.forwardsock = accept(m_listen_sock, (struct sockaddr *)&sin, &sinlen);

            if(os.forwardsock < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }
            if(os.forwardsock == -1) {
                PLOGE << "accept: " << strerror(errno);
                return false;
            }
            const char *src_host = inet_ntoa(sin.sin_addr);
            unsigned int src_port = ntohs(sin.sin_port);
        
            if(os.channel) {
                libssh2_channel_free(os.channel);
                os.channel = NULL;
                return false;
            }   
            if(establish_connection_to_bastion(os) == false) {
                return false;
            }
            PLOGV << "[host] trying to create a channel, dest " << m_dest_host << ":" << m_dest_port
                  << ", src " << src_host << ":"<<src_port;
            os.channel = libssh2_channel_direct_tcpip_ex(os.session, 
                                                         m_dest_host.c_str(), m_dest_port, 
                                                         src_host, src_port);
            if(!os.channel) {
                PLOGE << "[host] error: libssh2_channel_direct_tcpip_ex, failed to create a channel...";
                return false;
            }
            break;
        }
    }
    
    if(m_should_stop == false) {
        m_all_sessions.push_back(std::move(os));
        one_session& ros = m_all_sessions.back();
        ros.io_thread = std::thread([this, &ros]() {
            service_io(ros);
        });
    }
    return true; 
}


bool 
ssh_manager::ssh_tunnel::service_io(one_session& os)
{
    // must use non-blocking IO hereafter due to the current libssh2 API  
    libssh2_session_set_blocking(os.session, 0);

    fd_set fds;
    char buf[16384];
    struct timeval tv;

    while( true && m_should_stop == false) {
        FD_ZERO(&fds);
        FD_SET(os.forwardsock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int rc = select(os.forwardsock + 1, &fds, NULL, NULL, &tv);
        if(-1 == rc) {
            PLOGE << "select: " << strerror(errno);
            return false;
        }
        if(rc && FD_ISSET(os.forwardsock, &fds)) {
            ssize_t len = recv(os.forwardsock, buf, sizeof(buf), 0);
            if(len < 0) {
                PLOGE << "read: " << strerror(errno);
                return false;
            }
            else if(0 == len) {
                PLOGV << "[tunnel] the client disconnected" << std::endl; 
                return true;
            }
            ssize_t wr = 0;
            while((wr < len)  && m_should_stop == false)  {
                int i = libssh2_channel_write(os.channel, buf + wr, len - wr);

                if(LIBSSH2_ERROR_EAGAIN == i) {
                    continue;
                }
                if(i < 0) {
                    PLOGE << "[tunnel] error: libssh2_channel_write: "<< i << std::endl;
                    return false;
                }
                wr += i;
            }
        }
        while( m_should_stop == false) {
            ssize_t len = libssh2_channel_read(os.channel, buf, sizeof(buf));

            if(LIBSSH2_ERROR_EAGAIN == len)
                break;
            else if(len < 0) {
                PLOGE << "[tunnel] error: libssh2_channel_read: " << (int)len;
                return false;
            }
            ssize_t wr = 0;
            while((wr < len) && m_should_stop == false) {
                int i = send(os.forwardsock, buf + wr, len - wr, 0);
                if(i <= 0) {
                    PLOGE << "write: " << strerror(errno);
                    return false;
                }
                wr += i;
            }
            if(libssh2_channel_eof(os.channel)) {
                PLOGV << "[tunnel] the server disconnected";
                return true;
            }
        }
    }
    return true;
}




ssh_manager::ssh_manager()
{
    int rc = libssh2_init(0);
    if(rc) {
        PLOGV << "Error: libssh2 initialization failed: " << rc;
    }
}

ssh_manager::~ssh_manager()
{
    for(auto& tit : m_session_tunnels) {
        PLOGV << "Stopping ssh tunnel for session \'" << tit.first << "\'... ";        
        tit.second->stop();
        PLOGV << "done.";
    }
    libssh2_exit();
}

ssh_manager::ssh_tunnel_ret 
ssh_manager::start_ssh_tunnel(const std::string& session_name,
                              const std::string& bastion_username,
                              const std::string& bastion_key_file,
                              const std::string& desthost,
                              const unsigned int destport)
{
    PLOGV << "Starting ssh tunnel for session \'" << session_name << "\'... ";
    auto tunnel = std::make_unique<ssh_tunnel>(bastion_username, 
                                               bastion_key_file + ".pub",
                                               bastion_key_file,
                                               LOCAL_SSH_HOSTNAME,
                                               std::make_pair(LOCAL_SSH_PORT_START, LOCAL_SSH_PORT_START+1000),
                                               BASTION_SSH_HOSTNAME,
                                               BASTION_SSH_PORT,                                               
                                               desthost, 
                                               destport);
    auto tunnel_ret = tunnel->start();
    if(tunnel_ret.status) {
        m_session_tunnels.insert(std::make_pair(session_name, std::move(tunnel)));
    }
    return tunnel_ret;
}

void
ssh_manager::stop_ssh_tunnel(const std::string& name)
{
    auto fit = m_session_tunnels.find(name);
    if(fit != m_session_tunnels.end()) {
        PLOGV << "Stopping ssh tunnel for session \'" << name << "\'... ";
        fit->second->stop();
        m_session_tunnels.erase(fit);
        PLOGV << "done.";
    }
}

} // namespace metriffic
