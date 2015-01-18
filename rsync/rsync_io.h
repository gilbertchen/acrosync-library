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

#ifndef INCLUDED_RSYNC_IO_H
#define INCLUDED_RSYNC_IO_H

#include <string>

namespace rsync
{

// This class defines an interface for sending and receiving data through a network channel (which can be a plain
// socket or an SSH channel)
class IO
{
public:
    IO();
    virtual ~IO();

    // Create a communication channel.  'remoteCommand' is the rsync command to execute needed by an SSH connection.
    // '*protocol' gives the rsync protocol version.
    virtual void createChannel(const char *remoteCommand, int *protocol) = 0;

    // Close the channel (and this IO)
    virtual void closeChannel() = 0;

    // Return the account credentials previously stored in the object.
    virtual void getConnectInfo(std::string *username, std::string *password, std::string *module) = 0;
    
    // These two calls are blocking.
    virtual int read(char *buffer, int size) = 0;
    virtual int write(const char *buffer, int size) = 0;

    virtual void flush() = 0;

    // If the channel has been closed
    virtual bool isClosed() = 0;

    // Check if the channel is accessible.  Wait util the specified time elapsed.
    virtual bool isReadable(int timeoutInMilliSeconds) = 0;
    virtual bool isWritable(int timeoutInMilliSeconds) = 0;

private:
    // NOT IMPLEMENTED
    IO(const IO&);
    IO& operator=(const IO&);
};

} // namespace rsync
#endif //INCLUDED_RSYNC_IO_H
