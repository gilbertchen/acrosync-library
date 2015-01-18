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

#ifndef INCLUDED_RSYNC_SOCKETUTIL_H
#define INCLUDED_RSYNC_SOCKETUTIL_H

#include <sstream>

namespace rsync
{

struct SocketUtil
{
    // Methods for working with sockets.
    static void startup();
    static void cleanup();

    static int create(const char *host, int port, std::stringstream *error);
    static void close(int socket);

    static int read(int socket, char *buffer, int size);
    static int write(int socket, const char *buffer, int size);
    static bool isReadable(int socket, int timeoutInMilliSeconds);
    static bool isWritable(int socket, int timeoutInMilliSeconds);
};

} // close namespace rsync

#endif // INCLUDED_RSYNC_SOCKETUTIL_H
