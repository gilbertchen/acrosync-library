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

#include <rsync/rsync_client.h>

#include <rsync/rsync_entry.h>
#include <rsync/rsync_file.h>
#include <rsync/rsync_log.h>
#include <rsync/rsync_pathutil.h>
#include <rsync/rsync_socketutil.h>
#include <rsync/rsync_sshio.h>

#include <libssh2.h>
#include <openssl/md5.h>

#include <string>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <testutil/testutil_assert.h>
#include <testutil/testutil_newdeletemonitor.h>

//qi: TEST_PROGRAM = 1
//qi: LDFLAGS += -lssh2 -lssl -lcrypto -lz
#include <qi/qi_build.h>

int g_cancelFlag = 0;

#if defined(WIN32) || defined(__MINGW32__)

#include <Windows.h>
BOOL CtrlHandler( DWORD fdwCtrlType )
{
    if (fdwCtrlType == CTRL_C_EVENT) {
        g_cancelFlag = 1;
        return TRUE;
    }
    return FALSE;
}

#endif

using namespace rsync;

int main(int argc, char *argv[])
{
    TESTUTIL_INIT_RAND

    if (argc < 6) {
        return 0;
    }

#if defined(WIN32) || defined(__MINGW32__)
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    SocketUtil::startup();

    int rc = libssh2_init(0);
    if (rc != 0) {
        LOG_ERROR(LIBSSH2_INIT) << "libssh2 initialization failed: " << rc << LOG_END
        return 0;
    }

    const char *server = argv[1];
    const char *user = argv[2];
    const char *password = argv[3];

    // Make sure remoteDir ends with '/' to indicate it is a directory
    std::string remoteDir = argv[4]; 
    if (remoteDir.back() != '/') {
        remoteDir = remoteDir + "/";
    }

    std::string localDir = PathUtil::join(PathUtil::getCurrentDirectory().c_str(), argv[5]);  
    std::string temporaryFile = PathUtil::join(PathUtil::getCurrentDirectory().c_str(), "acrosync.part");  

    try {
        SSHIO sshio;
        sshio.connect(server, 22, user, password, 0, 0);

        Client client(&sshio, "rsync", 32, &g_cancelFlag);

        client.download(localDir.c_str(), remoteDir.c_str(), temporaryFile.c_str(), 0);
    } catch (Exception &e) {
        LOG_ERROR(RSYNC_ERROR) << "Sync failed: " << e.getMessage() << LOG_END
    }

    libssh2_exit();
    SocketUtil::cleanup();

    return 0;
}
