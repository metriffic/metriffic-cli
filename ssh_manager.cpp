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

#define HAVE_UNISTD_H
#define HAVE_INTTYPES_H
#define HAVE_STDLIB_H
#define HAVE_SYS_SELECT_H
#define HAVE_SYS_SOCKET_H
#define HAVE_SYS_TIME_H
#define HAVE_ARPA_INET_H
#define HAVE_NETINET_IN_H
#define HAVE_WINSOCK2_H

/* Functions */
#define HAVE_STRCASECMP
#define HAVE__STRICMP
#define HAVE_SNPRINTF
#define HAVE__SNPRINTF

/* Symbols */
#define HAVE___FUNC__
#define HAVE___FUNCTION__

namespace fs = std::experimental::filesystem;

namespace metriffic
{

ssh_manager::ssh_tunnel::ssh_tunnel(
               const std::string& username,
               const std::string& password,
               const std::string& local_host,
               std::pair<unsigned int, unsigned int> local_port_range,
               const std::string& bastion_host,
               const unsigned int bastion_port,
               const std::string& dest_host,
               const unsigned int dest_port) 
  : m_should_stop(false),
    m_username(username),
    m_password(password),
    m_local_host(local_host),
    m_local_port_range(local_port_range),
    m_bastion_host(bastion_host),
    m_bastion_port(bastion_port),
    m_dest_host(dest_host),
    m_dest_port(dest_port),
    m_channel(NULL),
    m_sock(-1),
    m_listensock(-1),
    m_forwardsock(-1)
{}

ssh_manager::ssh_tunnel::~ssh_tunnel() 
{
}



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
    if (err != 0) {}
        PLOGE << "Error: failed to get address to bastion: " << strerror(err);
        return sd;
    }

    for(struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
        sd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sd == -1) {
            err = errno;
            break; 
        }

        if (connect(sd, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }

        err = errno;

        close(sd);
        sd = -1;
    }

    freeaddrinfo(addrs);

    if (sd == -1) {
        PLOGE << "Error: failed to connect to bastion: " << strerror(err);
    }

    return sd;
}


ssh_manager::ssh_tunnel_ret
ssh_manager::ssh_tunnel::init()
{
    // tbd: take them out, make configurable
    constexpr unsigned int LOCAL_LISTEN_PORT_START = 3000;
    constexpr unsigned int LOCAL_LISTEN_PORT_END = 4000;

    // Connect to SSH server 
    m_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(m_sock == -1) { 
        PLOGE << "Error: failed to create a socket!";
        return ssh_tunnel_ret(false);
    }
 
    m_sock = connect_to_bastion();
 
    // Create a session instance 
    m_session = libssh2_session_init();

    if(!m_session) {
        PLOGE << "Error: could not initialize SSH session!";
        return ssh_tunnel_ret(false);
    }
 
    // Start it up. This will trade welcome banners, exchange keys,
    // and setup crypto, compression, and MAC layers
    int rc = libssh2_session_handshake(m_session, m_sock);

    if(rc) {
        PLOGE << "Error: failed to start up SSH session: " << rc;
        return ssh_tunnel_ret(false);
    }
 
    // At this point we havn't yet authenticated.  The first thing to do
    // is check the hostkey's fingerprint against our known hosts Your app
    // may have it hard coded, may go to a file, may present it to the
    // user, that's your call
    //const char* fingerprint = libssh2_hostkey_hash(m_session, LIBSSH2_HOSTKEY_HASH_SHA1);
    //std::cout<<"Fingerprint: ";
    //for(int i = 0; i < 20; i++) {
    //    std::cout << std::hex << ((unsigned char)fingerprint[i]) << " ";
    //}
    //std::cout<<std::endl;
 
    if(libssh2_userauth_password(m_session, m_username.c_str(), m_password.c_str())) {
        PLOGE << "Error: authentication by password failed!";
        return ssh_tunnel_ret(false);
    }
    PLOGV << "Authentication by password succeeded!";


    m_listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(m_listensock == -1) {
        //error("socket");
        return ssh_tunnel_ret(false);
    }

    m_sockopt = 1;
    setsockopt(m_listensock, SOL_SOCKET, SO_REUSEADDR, &m_sockopt, sizeof(m_sockopt));
    
    unsigned int listen_port = m_local_port_range.first-1;
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(m_local_host.c_str());
    socklen_t sinlen = sizeof(sin);
    if(INADDR_NONE == sin.sin_addr.s_addr) {
        PLOGE << "inet_addr: " << strerror(errno);
        return ssh_tunnel_ret(false);
    }
    while(listen_port++ < m_local_port_range.second) {
        sin.sin_port = htons(listen_port);
        if(-1 == bind(m_listensock, (struct sockaddr *)&sin, sinlen)) {
            PLOGE << "bind: " << strerror(errno);
            continue;
        }
        if(-1 == listen(m_listensock, 2)) {
            PLOGE << "listen: " << strerror(errno);
            continue;
        }
        break;
    }
//
    return ssh_tunnel_ret(true, listen_port, m_dest_host);
}

void 
ssh_manager::ssh_tunnel::stop()
{
    m_should_stop = true;
    close(m_forwardsock);
    close(m_listensock);
    if(m_channel) {
        libssh2_channel_free(m_channel);
    }
    libssh2_session_disconnect(m_session, "");//"Client disconnecting normally");
    libssh2_session_free(m_session);
    close(m_sock);  
    m_thread.join();
}

void 
ssh_manager::ssh_tunnel::run()
{
    m_thread = std::thread([this]() {
        struct sockaddr_in sin;
        socklen_t sinlen = sizeof(sin);

        while( !m_should_stop ) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 1000000;
            fd_set allset;
            int maxfd = m_listensock;		
            FD_ZERO(&allset);
            FD_SET(m_listensock, &allset);

            int nready = select(maxfd+1, &allset, NULL, NULL, &tv);

            if (FD_ISSET(m_listensock, &allset)) {
                //std::this_thread::sleep_for(std::chrono::milliseconds(500));

                m_forwardsock = accept(m_listensock, (struct sockaddr *)&sin, &sinlen);

                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                if(m_forwardsock == -1) {
                    PLOGE << "accept: " << strerror(errno);
                    break;
                }
            
                const char *shost = inet_ntoa(sin.sin_addr);
                unsigned int sport = ntohs(sin.sin_port);
            
                if(m_channel) {
                    libssh2_channel_free(m_channel);
                }
                m_channel = libssh2_channel_direct_tcpip_ex(m_session, 
                                                            m_dest_host.c_str(), m_dest_port, 
                                                            shost, sport);
                if(!m_channel) {
                    PLOGE << "libssh2_channel_direct_tcpip_ex: failed to create a channel...";
                    break;
                }
            
                // must use non-blocking IO hereafter due to the current libssh2 API  
                libssh2_session_set_blocking(m_session, 0);
                run_service(m_forwardsock, m_channel, shost, sport);
                // restoring back to blocking
                libssh2_session_set_blocking(m_session, 1);
            }
        }       
    
    });
}

bool 
ssh_manager::ssh_tunnel::run_service(int forwardsock, LIBSSH2_CHANNEL* channel,
                                     const char *shost, unsigned int sport)
{
    fd_set fds;
    char buf[16384];
    struct timeval tv;

    while( true && !m_should_stop) {
        FD_ZERO(&fds);
        FD_SET(forwardsock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int rc = select(forwardsock + 1, &fds, NULL, NULL, &tv);
        if(-1 == rc) {
            PLOGE << "select: " << strerror(errno);
            return false;
        }
        if(rc && FD_ISSET(forwardsock, &fds)) {
            ssize_t len = recv(forwardsock, buf, sizeof(buf), 0);
            if(len < 0) {
                PLOGE << "read: " << strerror(errno);
                return false;
            }
            else if(0 == len) {
                PLOGV << "the client at " << shost << ":" << sport << " disconnected!" << std::endl; 
                return true;
            }
            ssize_t wr = 0;
            while((wr < len)  && !m_should_stop)  {
                int i = libssh2_channel_write(channel, buf + wr, len - wr);

                if(LIBSSH2_ERROR_EAGAIN == i) {
                    continue;
                }
                if(i < 0) {
                    PLOGE << "Error: libssh2_channel_write: "<< i << std::endl;
                    return false;
                }
                wr += i;
            }
        }
        while( true  && !m_should_stop) {
            ssize_t len = libssh2_channel_read(channel, buf, sizeof(buf));

            if(LIBSSH2_ERROR_EAGAIN == len)
                break;
            else if(len < 0) {
                PLOGE << "Error: libssh2_channel_read: " << (int)len;
                return false;
            }
            ssize_t wr = 0;
            while((wr < len) && !m_should_stop) {
                int i = send(forwardsock, buf + wr, len - wr, 0);
                if(i <= 0) {
                    PLOGE << "write: " << strerror(errno);
                    return false;
                }
                wr += i;
            }
            if(libssh2_channel_eof(channel)) {
                PLOGV << "The server disconnected";
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
                              const std::string& desthost,
                              const unsigned int destport)
{
    PLOGV << "Starting ssh tunnel for session \'" << session_name << "\'... ";
    auto tunnel = std::make_unique<ssh_tunnel>(BASTION_SSH_USERNAME, 
                                               BASTION_SSH_PASSWORD, 
                                               LOCAL_SSH_HOSTNAME,
                                               std::make_pair(LOCAL_SSH_PORT_START, LOCAL_SSH_PORT_START+1000),
                                               BASTION_SSH_HOSTNAME,
                                               BASTION_SSH_PORT,                                               
                                               desthost, 
                                               destport);
    auto tunnel_ret = tunnel->init();
    if(tunnel_ret.status) {
        tunnel->run();
        m_session_tunnels.insert(std::make_pair(session_name, std::move(tunnel)));
        PLOGV << "done.";
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
