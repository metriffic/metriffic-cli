#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "ssh_manager.hpp"









#include <libssh2.h>
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
 
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
 
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
//#ifndef INADDR_NONE
//#define INADDR_NONE (in_addr_t)-1
//#endif













namespace fs = std::experimental::filesystem;

namespace metriffic
{

ssh_manager::ssh_manager()
{
}

bool 
ssh_manager::start_ssh_tunnel(const std::string& username,
                              const std::string& password,
                              const std::string& desthost,
                              const unsigned int destport)
{
    struct state {
        ~state() {
            close(forwardsock);
            close(listensock);
            if(channel) {
                libssh2_channel_free(channel);
            }
            libssh2_session_disconnect(session, "Client disconnecting normally");
            libssh2_session_free(session);
            close(sock);
            libssh2_exit();
        }
        int rc;
        struct sockaddr_in sin;
        
        LIBSSH2_SESSION *session;
        LIBSSH2_CHANNEL *channel = NULL;
    
        int sockopt, sock = -1;
        int listensock = -1, forwardsock = -1;
    } st;

    const std::string SERVER_IP = "127.0.0.1";
    const std::string LOCAL_LISTEN_IP = "127.0.0.1";
    unsigned int LOCAL_LISTEN_PORT = 3000;

    int rc = libssh2_init(0);

    if(rc) {
        //std::cout<<"Error: libssh2 initialization failed: "<<rc<<std::endl;
        return false;
    }
 
    // Connect to SSH server 
    st.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(st.sock == -1) {        
        return false;
    }
 
    st.sin.sin_family = AF_INET;
    st.sin.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());
    if(INADDR_NONE == st.sin.sin_addr.s_addr) {
        return false;
    }
    st.sin.sin_port = htons(22);
    if(connect(st.sock, (struct sockaddr*)(&st.sin),
               sizeof(struct sockaddr_in)) != 0) {
        std::cout << "Error: failed to connect!" << std::endl;
        return false;
    }
 
    // Create a session instance 
    st.session = libssh2_session_init();

    if(!st.session) {
        std::cout << "Error: could not initialize SSH session!" << std::endl;
        return false;
    }
 
    // Start it up. This will trade welcome banners, exchange keys,
    // and setup crypto, compression, and MAC layers
    rc = libssh2_session_handshake(st.session, st.sock);

    if(rc) {
        std::cout << "Error: failed to start up SSH session: " << rc <<std::endl;
        return false;
    }
 
    // At this point we havn't yet authenticated.  The first thing to do
    // is check the hostkey's fingerprint against our known hosts Your app
    // may have it hard coded, may go to a file, may present it to the
    // user, that's your call
    const char* fingerprint = libssh2_hostkey_hash(st.session, LIBSSH2_HOSTKEY_HASH_SHA1);

    std::cout<<"Fingerprint: ";
    for(int i = 0; i < 20; i++) {
        std::cout << std::hex << ((unsigned char)fingerprint[i]) << " ";
    }
    std::cout<<std::endl;
 
    if(libssh2_userauth_password(st.session, username.c_str(), password.c_str())) {
        std::cout << "Error: authentication by password failed!" << std::endl;
        return false;
    }
    std::cout << "Authentication by password succeeded!" << std::endl;


    st.listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(st.listensock == -1) {
        //error("socket");
        return false;
    }

 
    st.sin.sin_family = AF_INET;
    st.sin.sin_port = htons(LOCAL_LISTEN_PORT);
    st.sin.sin_addr.s_addr = inet_addr(LOCAL_LISTEN_IP.c_str());
    if(INADDR_NONE == st.sin.sin_addr.s_addr) {
        //perror("inet_addr");
        return false;
    }
    st.sockopt = 1;
    setsockopt(st.listensock, SOL_SOCKET, SO_REUSEADDR, &st.sockopt,
               sizeof(st.sockopt));
    socklen_t sinlen = sizeof(st.sin);
    if(-1 == bind(st.listensock, (struct sockaddr *)&st.sin, sinlen)) {
        //perror("bind");
        return false;
    }
    if(-1 == listen(st.listensock, 2)) {
        //perror("listen");
        return false;
    }
 
    std::cout << "Waiting for TCP connection on "<< inet_ntoa(st.sin.sin_addr) << ":" 
              << ntohs(st.sin.sin_port) << "..." << std::endl;
 
    st.forwardsock = accept(st.listensock, (struct sockaddr *)&st.sin, &sinlen);

    if(st.forwardsock == -1) {
        //perror("accept");
        return false;
    }
 
    const char *shost = inet_ntoa(st.sin.sin_addr);
    unsigned int sport = ntohs(st.sin.sin_port);
 
    std::cout << "Forwarding connection from "
        << shost << ":"  << sport
        << " here to remote "
        << desthost << ":" << destport <<std::endl;
 
    st.channel = libssh2_channel_direct_tcpip_ex(st.session, 
                                              desthost.c_str(), destport, 
                                              shost, sport);
    if(!st.channel) {
        std::cout << "Error: could not open the direct-tcpip channel!" << std::endl
                 << "(Note that this can be a problem at the server!"
                 << " Please review the server logs.)" << std::endl;
        return false;
    }
 
    /* Must use non-blocking IO hereafter due to the current libssh2 API */ 
    libssh2_session_set_blocking(st.session, 0);

    fd_set fds;
    ssize_t len, wr;
    char buf[16384];
    struct timeval tv;

    while(1) {
        FD_ZERO(&fds);
        FD_SET(st.forwardsock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        rc = select(st.forwardsock + 1, &fds, NULL, NULL, &tv);
        if(-1 == rc) {
            //perror("select");
            return false;
        }
        if(rc && FD_ISSET(st.forwardsock, &fds)) {
            len = recv(st.forwardsock, buf, sizeof(buf), 0);
            if(len < 0) {
                //perror("read");
                return false;
            }
            else if(0 == len) {
                std::cout << "Error: the client at " << shost << ":" << sport << "disconnected!" << std::endl; 
                return false;
            }
            wr = 0;
            while(wr < len) {
                int i = libssh2_channel_write(st.channel, buf + wr, len - wr);

                if(LIBSSH2_ERROR_EAGAIN == i) {
                    continue;
                }
                if(i < 0) {
                    std::cout << "Error: libssh2_channel_write: "<< i << std::endl;
                    return false;
                }
                wr += i;
            }
        }
        while(1) {
            len = libssh2_channel_read(st.channel, buf, sizeof(buf));

            if(LIBSSH2_ERROR_EAGAIN == len)
                break;
            else if(len < 0) {
                std::cout << "Error: libssh2_channel_read: "<< (int)len << std::endl;
                return false;
            }
            wr = 0;
            while(wr < len) {
                int i = send(st.forwardsock, buf + wr, len - wr, 0);
                if(i <= 0) {
                    //perror("write");
                    return false;
                }
                wr += i;
            }
            if(libssh2_channel_eof(st.channel)) {

                std::cout << "The server at "
                    << desthost << ":" << destport
                    << "disconnected!" << std::endl;
                return true;
            }
        }
    }
    return true;
}


} // namespace metriffic
