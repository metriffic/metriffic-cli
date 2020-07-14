#include <nlohmann/json.hpp>
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
               const std::string& desthost,
               const unsigned int destport) 
  : m_should_stop(false),
    m_username(username),
    m_password(password),
    m_desthost(desthost),
    m_destport(destport),
    m_channel(NULL),
    m_sock(-1),
    m_listensock(-1),
    m_forwardsock(-1)
{}

ssh_manager::ssh_tunnel::~ssh_tunnel() 
{
}

std::pair<bool, unsigned int> 
ssh_manager::ssh_tunnel::init()
{
    const std::string SERVER_IP = "127.0.0.1";
    const std::string LOCAL_LISTEN_IP = "127.0.0.1";
    unsigned int LOCAL_LISTEN_PORT_START = 3000;
    unsigned int LOCAL_LISTEN_PORT_END = 4000;

    // Connect to SSH server 
    m_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(m_sock == -1) {        
        return {false, -1};
    }
 
    m_sin.sin_family = AF_INET;
    m_sin.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());
    if(INADDR_NONE == m_sin.sin_addr.s_addr) {
        return {false, -1};
    }
    m_sin.sin_port = htons(22);
    if(connect(m_sock, (struct sockaddr*)(&m_sin),
               sizeof(struct sockaddr_in)) != 0) {
        //std::cout << "Error: failed to connect!" << std::endl;
        return {false, -1};
    }
 
    // Create a session instance 
    m_session = libssh2_session_init();

    if(!m_session) {
        //std::cout << "Error: could not initialize SSH session!" << std::endl;
        return {false, -1};
    }
 
    // Start it up. This will trade welcome banners, exchange keys,
    // and setup crypto, compression, and MAC layers
    int rc = libssh2_session_handshake(m_session, m_sock);

    if(rc) {
        //std::cout << "Error: failed to start up SSH session: " << rc <<std::endl;
        return {false, -1};
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
        //std::cout << "Error: authentication by password failed!" << std::endl;
        return {false, -1};
    }
    //std::cout << "Authentication by password succeeded!" << std::endl;


    m_listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(m_listensock == -1) {
        //error("socket");
        return {false, -1};
    }

    m_sockopt = 1;
    setsockopt(m_listensock, SOL_SOCKET, SO_REUSEADDR, &m_sockopt, sizeof(m_sockopt));
    
    unsigned int listen_port = LOCAL_LISTEN_PORT_START-1;
    m_sin.sin_family = AF_INET;
    m_sin.sin_addr.s_addr = inet_addr(LOCAL_LISTEN_IP.c_str());
    socklen_t sinlen = sizeof(m_sin);
    if(INADDR_NONE == m_sin.sin_addr.s_addr) {
        //perror("inet_addr");
        return {false, -1};
    }
    while(listen_port++ < LOCAL_LISTEN_PORT_END) {
        m_sin.sin_port = htons(listen_port);
        if(-1 == bind(m_listensock, (struct sockaddr *)&m_sin, sinlen)) {
            //perror("bind");
            continue;
        }
        if(-1 == listen(m_listensock, 2)) {
            //perror("listen");
            continue;
        }
        break;
    }
//
    return {true, listen_port};
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
        socklen_t sinlen = sizeof(m_sin);

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

                m_forwardsock = accept(m_listensock, (struct sockaddr *)&m_sin, &sinlen);

                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
                if(m_forwardsock == -1) {
                    //perror("accept");
                    break;
                }
            
                const char *shost = inet_ntoa(m_sin.sin_addr);
                unsigned int sport = ntohs(m_sin.sin_port);
            
                if(m_channel) {
                    libssh2_channel_free(m_channel);
                }
                m_channel = libssh2_channel_direct_tcpip_ex(m_session, 
                                                            m_desthost.c_str(), m_destport, 
                                                            shost, sport);
                if(!m_channel) {
                    break;
                }
            
                // must use non-blocking IO hereafter due to the current libssh2 API  
                libssh2_session_set_blocking(m_session, 0);
                run_service(m_forwardsock, m_channel);
                // restoring back to blocking
                libssh2_session_set_blocking(m_session, 1);
            }
        }       
    
    });
}

bool 
ssh_manager::ssh_tunnel::run_service(int forwardsock, LIBSSH2_CHANNEL* channel)
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
            //perror("select");
            return false;
        }
        if(rc && FD_ISSET(forwardsock, &fds)) {
            ssize_t len = recv(forwardsock, buf, sizeof(buf), 0);
            if(len < 0) {
                //perror("read");
                return false;
            }
            else if(0 == len) {
                //std::cout << "Error: the client at " /*<< shost << ":" << sport */<< "disconnected!" << std::endl; 
                return true;
            }
            ssize_t wr = 0;
            while((wr < len)  && !m_should_stop)  {
                int i = libssh2_channel_write(channel, buf + wr, len - wr);

                if(LIBSSH2_ERROR_EAGAIN == i) {
                    continue;
                }
                if(i < 0) {
                    //std::cout << "Error: libssh2_channel_write: "<< i << std::endl;
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
                //std::cout << "Error: libssh2_channel_read: "<< (int)len << std::endl;
                return false;
            }
            ssize_t wr = 0;
            while((wr < len) && !m_should_stop) {
                int i = send(forwardsock, buf + wr, len - wr, 0);
                if(i <= 0) {
                    //perror("write");
                    return false;
                }
                wr += i;
            }
            if(libssh2_channel_eof(channel)) {
                //std::cout << "The server disconnected" << std::endl;
                return true;
            }
        }
    }
}




ssh_manager::ssh_manager()
{
    int rc = libssh2_init(0);
    if(rc) {
        std::cout<<"Error: libssh2 initialization failed: "<<rc<<std::endl;
    }
}

ssh_manager::~ssh_manager()
{
    for(auto& tit : m_session_tunnels) {
        //std::cout<<"Stopping ssh tunnel for session \'"<<tit.first<<"\'... ";
        tit.second->stop();
        //std::cout<<"done."<<std::endl;
    }
    libssh2_exit();
}

std::pair<bool, unsigned int> 
ssh_manager::start_ssh_tunnel(const std::string& session_name,
                              const std::string& username,
                              const std::string& password,
                              const std::string& desthost,
                              const unsigned int destport)
{
    //std::cout<<"Starting ssh tunnel for session \'"<<session_name<<"\'... ";
    auto tunnel = std::make_unique<ssh_tunnel>(username, password, desthost, destport);
    auto ret = tunnel->init();
    if(ret.first) {
        tunnel->run();
        m_session_tunnels.insert(std::make_pair(session_name, std::move(tunnel)));
        //std::cout<<"done."<<std::endl;
    }
    return ret;
}

void
ssh_manager::stop_ssh_tunnel(const std::string& name)
{
    auto fit = m_session_tunnels.find(name);
    if(fit != m_session_tunnels.end()) {
        //std::cout<<"Stopping ssh tunnel for session \'"<<name<<"\'... ";
        fit->second->stop();
        m_session_tunnels.erase(fit);
        //std::cout<<"done."<<std::endl;
    }
}

} // namespace metriffic
