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

#ifndef INCLUDED_RSYNC_SOCKETIO_H
#define INCLUDED_RSYNC_SOCKETIO_H

#include <rsync/rsync_io.h>

#include <list>
#include <string>

#include <stdint.h>

namespace rsync
{

// Implementation of the IO interface using a socket.
class SocketIO : public IO
{
public:
    SocketIO();
    ~SocketIO();

    void connect(const char *serverList, int port, const char *user, const char *password, const char *module);
    void getConnectInfo(std::string *username, std::string *password, std::string *module);
    void setModule(const char *module);
    
    void createChannel(const char *remoteCommand, int *protocol);
    void closeChannel();

    virtual int read(char *buffer, int size);
    virtual int write(const char *buffer, int size);
    virtual void flush();
    virtual bool isClosed();

    virtual bool isReadable(int timeoutInMilliSeconds);
    virtual bool isWritable(int timeoutInMilliSeconds);
    
private:
    // NOT IMPLEMENTED
    SocketIO(const SocketIO&);
    SocketIO& operator=(const SocketIO&);

    int d_socket;

    std::string d_serverList;
    int d_port;
    std::string d_user;
    std::string d_password;
    std::string d_module;
    
};

} // namespace rsync

#endif // INCLUDED_RSYNC_SOCKETIO_H
