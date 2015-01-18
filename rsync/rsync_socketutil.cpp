// Copyright (C) 2015 Acrosync LLC
//
// Unless explicitly acquired and licensed from Licensor under another
// license, the contents of this file are subject to the Reciprocal Public
// License ("RPL") Version 1.5, or subsequent versions as allowed by the RPL,
// and You may not copy or use this file in either source code or executable
// form, except in compliance with the terms and conditions of the RPL.
//
// All software distributed under the RPL is provided strictly on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, AND
// LICENSOR HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
// LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, QUIET ENJOYMENT, OR NON-INFRINGEMENT. See the RPL for specific
// language governing rights and limitations under the RPL. 

#if defined(WIN32) || defined(__MINGW32__)
#define _WIN32_WINNT 0x501 // means windows XP and above
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#ifdef __APPLE__
#include <sys/select.h>
#endif
#define SOCKADDR sockaddr
#define SOCKET_ERROR -1
#endif

#include <rsync/rsync_socketutil.h>
#include <rsync/rsync_util.h>

#include <rsync/rsync_log.h>

#include <qi/qi_build.h>

namespace rsync
{

void SocketUtil::startup()
{
#if defined(WIN32) || defined(__MINGW32__)
    static WSADATA wsaData = { 0 };
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_FATAL(SOCKET_STARTUP) << "WSAStartup failed: " << result << LOG_END
    }
#endif // WIN32
}

void SocketUtil::cleanup()
{
#if defined(WIN32) || defined(__MINGW32__)
    WSACleanup();
#endif // WIN32
}

int SocketUtil::create(const char *host, int port, std::stringstream *error)
{
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *info = 0;
    int result = getaddrinfo(host, "22", &hints, &info);
    if (result != 0) {
        *error << "Failed to resolve the host '" << host << "': " << result;
        return -1;
    }
    struct addrinfo *ptr = info;
    for (; ptr != 0; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6) {
            break;
        } 
    }

    if (!ptr) {
        freeaddrinfo(info);
        *error << "Failed to resolve the host '" << host << "' into an ip address";
        return -1;
    }

    sockaddr_storage addr;
    sockaddr_in6 *addr_v6 = 0;
    int addrlen = ptr->ai_addrlen;
    ::memcpy(&addr, ptr->ai_addr, ptr->ai_addrlen);
    if (ptr->ai_family == AF_INET) {
        sockaddr_in *addr_v4 = (sockaddr_in*)&addr;
        addr_v4->sin_port = htons(port);
    } else {
        addr_v6 = (sockaddr_in6*)&addr;
        addr_v6->sin6_port = htons(port);
    }
    freeaddrinfo(info);

    int sock = socket(addr_v6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if defined(WIN32) || defined(__MINGW32__)
    if (sock == static_cast<int>(INVALID_SOCKET)) {
        *error << "Failed to create the socket: " << WSAGetLastError();
        return -1;
    }
#else
    if (sock == -1) {
        *error << "Failed to create the socket: " << strerror(errno);
        return -1;
    }
    
#ifdef __APPLE__
    // Ignore SIGPIPE
    int value = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&value, sizeof(value));
#endif
    
#endif
    
    
#if defined(WIN32) || defined(__MINGW32__)
    u_long iMode=1;
    result = ioctlsocket(sock, FIONBIO, &iMode);
    if (result != NO_ERROR) {
        *error << "Failed to enable nonblocking mode: " << result;
        close(sock);
        return -1;
    }
#else
    fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
    
#if defined(WIN32)
    int bufferLength = 1024 * 1024;
    result = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufferLength, sizeof(bufferLength));
    if (result == SOCKET_ERROR) {
        LOG_ERROR(SOCKET_SNDBUF) << "Failed to change the TCP send buffer size: " << WSAGetLastError() << LOG_END
    }
    result = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufferLength, sizeof(bufferLength));
    if (result == SOCKET_ERROR) {
        LOG_ERROR(SOCKET_SNDBUF) << "Failed to change the TCP receive buffer size: " << WSAGetLastError() << LOG_END
    }
#else
    int bufferLength = 128 * 1024;
    result = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufferLength, sizeof(bufferLength));
    if (result < 0) {
        LOG_ERROR(SOCKET_SNDBUF) << "Failed to change the TCP send buffer size: " << strerror(errno) << LOG_END
    }
    result = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufferLength, sizeof(bufferLength));
    if (result < 0) {
        LOG_ERROR(SOCKET_SNDBUF) << "Failed to change the TCP receive buffer size: " << strerror(errno) << LOG_END
    }

    /*int flag = 1;
    result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
    if (result < 0) {
        LOG_ERROR(SOCKET_NODELAY) << "Failed to set the TCP_NODELAY option: " << strerror(errno) << LOG_END
    }*/
#endif
    
    result = connect(sock, reinterpret_cast<SOCKADDR*>(&addr), addrlen);
    
    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    result = select(sock + 1, NULL, &fdset, NULL, &tv);

    if (result < 0) {
        *error << "Failed to connect to '" << host << ":" << port << "': code " << result;
        close(sock);
        return -1;
    } else if (result == 0) {
        *error << "Failed to connect to '" << host << ":" << port << "': connection timeout ";
        close(sock);
        return -1;
    }

    return sock;
}

void SocketUtil::close(int socket)
{
#if defined(WIN32) || defined(__MINGW32__)
    shutdown(socket, SD_BOTH);
#else
    ::close(socket);
#endif
}

#if defined(WIN32) || defined(__MINGW32__)

int SocketUtil::read(int socket, char *buffer, int size)
{
    int rc = ::recv(socket, buffer, size, 0);
    if (rc == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            LOG_ERROR(RSYNC_SOCKET) << "Error reading from socket: " << Util::getLastError() << LOG_END
            return -1;
        } else {
            return 0;
        }
    } else if (rc == 0) {
        LOG_ERROR(RSYNC_SOCKET) << "Socket was closed unexpectedly" << LOG_END
        return -1;
    }
    return rc;
}

int SocketUtil::write(int socket, const char *buffer, int size)
{
    int rc = ::send(socket, buffer, size, 0);
    if (rc == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            LOG_ERROR(RSYNC_SOCKET) << "Error writing to socket: " << Util::getLastError() << LOG_END
            return -1;
        } else {
            return 0;
        }
    } else if (rc == 0) {
        LOG_ERROR(RSYNC_SOCKET) << "Socket was closed unexpectedly" << LOG_END
        return -1;
    }
    return rc;
}

#else 

int SocketUtil::read(int socket, char *buffer, int size)
{
    int rc = static_cast<int>(::read(socket, buffer, size));
    if (rc == -1) {
        if (errno != EAGAIN) {
            LOG_ERROR(RSYNC_SOCKET) << "Error reading from socket: " << Util::getLastError() << LOG_END
            return -1;
        } else {
            return 0;
        }
    } else if (rc == 0) {
        LOG_ERROR(RSYNC_SOCKET) << "Socket was closed unexpectedly" << LOG_END
        return -1;
    }
    return rc;
}

int SocketUtil::write(int socket, const char *buffer, int size)
{
    int rc = static_cast<int>(::write(socket, buffer, size));
    if (rc == -1) {
        if (errno != EAGAIN) {
            LOG_ERROR(RSYNC_SOCKET) << "Error writing to socket: " << Util::getLastError() << LOG_END
            return -1;
        } else {
            return 0;
        }
    } else if (rc == 0) {
        LOG_ERROR(RSYNC_SOCKET) << "Socket was closed unexpectedly" << LOG_END
        return -1;
    }
    return rc;
}
#endif

bool SocketUtil::isReadable(int socket, int timeoutInMilliSeconds)
{
    struct timeval tv;
    tv.tv_sec = timeoutInMilliSeconds / 1000;
    tv.tv_usec = (timeoutInMilliSeconds % 1000) * 1000;
  
    fd_set readFDs; 
    FD_ZERO(&readFDs);
    FD_SET(static_cast<unsigned int>(socket), &readFDs);

    return select(socket + 1, &readFDs, 0, 0, &tv);
}

bool SocketUtil::isWritable(int socket, int timeoutInMilliSeconds)
{
    struct timeval tv;
    tv.tv_sec = timeoutInMilliSeconds / 1000;
    tv.tv_usec = (timeoutInMilliSeconds % 1000) * 1000;
  
    fd_set writeFDs; 
    FD_ZERO(&writeFDs);
    FD_SET(static_cast<unsigned int>(socket), &writeFDs);

    return select(socket + 1, 0, &writeFDs, 0, &tv);
}


} // close namespace rsync
