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

#include <rsync/rsync_socketio.h>

#include <rsync/rsync_log.h>
#include <rsync/rsync_socketutil.h>

#include <qi/qi_build.h>

#include <vector>

#include <sstream>

#include <cstdio>
#include <cstdlib>


namespace rsync
{

SocketIO::SocketIO()
    : IO()
    , d_socket(-1)
    , d_serverList()
    , d_port(0)
    , d_user()
    , d_password()
    , d_module()
{
}

SocketIO::~SocketIO()
{
    closeChannel();
}

void SocketIO::connect(const char *serverList, int port, const char *user, const char *password, const char *module)
{
    d_serverList = serverList;
    d_port = port;
    d_user = user;
    d_password = password;
    d_module = module;
}
    
void SocketIO::getConnectInfo(std::string *username, std::string *password, std::string *module)
{
    *username = d_user;
    *password = d_password;
    *module = d_module;
}

void SocketIO::setModule(const char *module)
{
    d_module = module;
}
    
void SocketIO::createChannel(const char *remoteCommand, int *protocol)
{
    closeChannel();
    
    std::stringstream error;
    d_socket = rsync::SocketUtil::create(d_serverList.c_str(), d_port, &error);
    if (d_socket == -1) {
        LOG_FATAL(SOCKET_CONNECT) << error.str() << LOG_END
        return;
    }
}
    
void SocketIO::closeChannel()
{
    if (d_socket != -1) {
        SocketUtil::close(d_socket);
        d_socket = -1;
    }
}

int SocketIO::read(char *buffer, int size)
{
    return SocketUtil::read(d_socket, buffer, size);
}

int SocketIO::write(const char *buffer, int size)
{
    return SocketUtil::write(d_socket, buffer, size);
}

void SocketIO::flush()
{
}

bool SocketIO::isClosed()
{
    return false;
}
    
bool SocketIO::isReadable(int timeoutInMilliSeconds)
{
    return SocketUtil::isReadable(d_socket, timeoutInMilliSeconds);
        
}

bool SocketIO::isWritable(int timeoutInMilliSeconds)
{
    return SocketUtil::isWritable(d_socket, timeoutInMilliSeconds);
}
        
} // namespace rsync
