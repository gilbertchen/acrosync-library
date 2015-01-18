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

#ifndef INCLUDED_RSYNC_SSHIO_H
#define INCLUDED_RSYNC_SSHIO_H

#include <rsync/rsync_io.h>

#include <block/block_out.h>

#include <list>
#include <string>

#include <stdint.h>

struct _LIBSSH2_SESSION;
struct _LIBSSH2_CHANNEL;

namespace rsync
{

// Implementation of the IO interface on top of an SSH session.
class SSHIO : public IO
{
public:
    SSHIO();
    ~SSHIO();

    static bool startup();
    static void cleanup();

    void connect(const char *serverList, int port, const char *user, const char *password,  const char *keyFile, const char *hostKey);
    void getConnectInfo(std::string *username, std::string *password, std::string *module);
    
    void createChannel(const char *remoteCommand, int *protocol);
    void closeChannel();
    
    void closeSession();

    virtual int read(char *buffer, int size);
    virtual int write(const char *buffer, int size);
    virtual void flush();
    virtual bool isClosed();
    virtual bool isReadable(int timeoutInMilliSeconds);
    virtual bool isWritable(int timeoutInMilliSeconds);

    bool isConnected() const {
        return d_session != 0;
    }

    std::string getLastError();

    void dump();

    block::out<bool(const char*, const char*)> hostKeyOut;
    
private:
    // NOT IMPLEMENTED
    SSHIO(const SSHIO&);
    SSHIO& operator=(const SSHIO&);

    bool isReadable();
    void checkError(int rc);

    int d_socket;
    _LIBSSH2_SESSION *d_session;
    _LIBSSH2_CHANNEL *d_channel;

    std::list<uint64_t> d_recentWrites;
};

} // namespace rsync

#endif // INCLUDED_RSYNC_SSHIO_H
