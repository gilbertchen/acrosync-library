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

#include <rsync/rsync_sshio.h>

#include <rsync/rsync_log.h>
#include <rsync/rsync_socketutil.h>
#include <rsync/rsync_timeutil.h>
#include <rsync/rsync_util.h>

#include <libssh2.h>

#include <qi/qi_build.h>

#include <vector>

#include <sstream>

#include <cstdio>
#include <cstdlib>

bool libssh2_debug_enabled = true;

extern "C"
void libssh2_debug_handler(LIBSSH2_SESSION* /*session*/,
                           void* /*context*/,
                           const char *buffer,
                           size_t /*length*/)
{
    if (libssh2_debug_enabled) {
        LOG_INFO(LIBSSH2_DEBUG) << buffer << LOG_END
    }
}

namespace rsync
{

namespace {

const char *g_password = 0;

void keyboardCallback(const char *name, int nameLength,
                      const char *instruction, int instructionLength, int numPrompts,
                      const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
                      LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
                      void **abstract)
{
    (void)name;
    (void)nameLength;
    (void)instruction;
    (void)instructionLength;
    if (numPrompts == 1) {
        responses[0].text = ::strdup(g_password);
        responses[0].length = ::strlen(g_password);
    }
    (void)prompts;
    (void)abstract;
}

} // unnamed namesapce

bool SSHIO::startup()
{
    return 0 == libssh2_init(0);
}

void SSHIO::cleanup()
{
    libssh2_exit();
}

SSHIO::SSHIO()
    : IO()
    , d_socket(0)
    , d_session(0)
    , d_channel(0)
{
}

SSHIO::~SSHIO()
{
    closeSession();
}

void SSHIO::connect(const char *serverList, int port, const char *user, const char *password, const char *keyFile, const char *hostKey)
{
    closeSession();

    std::vector<std::string> servers;
    std::string serverString = serverList;

    Util::tokenize(serverString, &servers, ";, \t");
    
    if (servers.size() == 0) {
        LOG_FATAL(SSH_INIT) << "No server specified" << LOG_END
    }
    
    unsigned int i;
    if (hostKey) {
        // Only use servers[0];
        i = 0;
        std::stringstream error;
        d_socket = SocketUtil::create(servers[i].c_str(), port, &error);
        if (d_socket == -1) {
            LOG_FATAL(SOCKET_CONNECT) << error.str() << LOG_END
        }        
    } else {
        for (i = 0; i < servers.size(); ++i) {
            std::stringstream error;
            d_socket = SocketUtil::create(servers[i].c_str(), port, &error);
            if (d_socket == -1) {
                if (i == servers.size() - 1) {
                    LOG_FATAL(SOCKET_CONNECT) << error.str() << LOG_END
                } else {
                    LOG_ERROR(SOCKET_CONNECT) << error.str() << LOG_END
                    
                }
            } else {
                break;
            }
        }
    }

    d_session = libssh2_session_init();
    if (!d_session) {
        LOG_FATAL(SSH_INIT) << "Failed to create a libssh2 session" << LOG_END
    }

    //libssh2_trace_sethandler(d_session, 0, libssh2_debug_handler);
    //libssh2_trace(d_session, LIBSSH2_TRACE_TRANS /*| LIBSSH2_TRACE_CONN */| LIBSSH2_TRACE_ERROR | LIBSSH2_TRACE_SOCKET);

    libssh2_session_set_blocking(d_session, 1);

    libssh2_session_set_timeout(d_session, 100000);

    int rc = libssh2_session_startup(d_session, d_socket);
    if (rc != 0) {
        LOG_FATAL(SSH_START) << "Failed to establish an SSH session to '" << servers[i] << ":" << port << "': " << getLastError() << LOG_END
        return;
    }

    if (hostKeyOut.isConnected() || hostKey) {
        const char *fingerPrint = libssh2_hostkey_hash(d_session, LIBSSH2_HOSTKEY_HASH_SHA1);
        std::string hash;
        const char *hex = "0123456789ABCDEF";

        for (unsigned int j = 0; j < 20; ++j) {
            unsigned char c = fingerPrint[j];
            if (j != 0) {
                hash += ":";
            }
            hash += hex[c / 16];
            hash += hex[c % 16];
        }
        
        if (hostKeyOut.isConnected()) {
            if (!hostKeyOut(servers[i].c_str(), hash.c_str())) {
                LOG_FATAL(SSH_HOSTKEY) << "New host key for server '" << servers[i] << "' is not accepted" << LOG_END
            }
        } else {
            if (hash != hostKey) {
                LOG_FATAL(SSH_HOSTKEY) << hash << " : new host key for server '" << servers[i] << ";" << LOG_END;
                return;
            }
        }
        
    }

    const char *methods = libssh2_userauth_list(d_session, user, ::strlen(user));

    if (methods == NULL) {
        LOG_FATAL(SSH_AUTH) << "User '" << user << "' is not allowed to log in to '"
                            << servers[i] << "'" << LOG_END
        return;
    }

    if (keyFile != 0 && *keyFile != 0) {
        if (::strstr(methods, "publickey") == NULL) {
            LOG_FATAL(SSH_AUTH) << "Public key authorization is not permitted by the server" << LOG_END
            return;
        }

        rc = libssh2_userauth_publickey_fromfile(d_session, user, NULL, keyFile, password);

        if (rc != 0) {
            LOG_FATAL(SSH_AUTH) << "Authentication by public key failed: " << getLastError() << LOG_END
            return;
        }
    } else if (::strstr(methods, "password") != NULL) {
        rc = libssh2_userauth_password(d_session, user, password);
        if (rc != 0) {
            LOG_FATAL(SSH_AUTH) << "Invalid username or password: " << getLastError() << LOG_END
            return;
        }
    } else if (::strstr(methods, "keyboard-interactive") != NULL) {
        g_password = password;
        rc = libssh2_userauth_keyboard_interactive(d_session, user, &keyboardCallback);
        if (rc != 0) {
            LOG_FATAL(SSH_AUTH) << "Invalid username/password: " << getLastError() << LOG_END
            return;
        }
    } else {
        LOG_FATAL(SSH_AUTH) << "Authentication by password not supported" << LOG_END
        return;
    }

    libssh2_keepalive_config (d_session, 1, 5);
}

void SSHIO::getConnectInfo(std::string * /*username*/, std::string * /*password*/, std::string * /*module*/)
{
    LOG_FATAL(SSH_NOT_IMPLEMENTED) << "This 'getConnectInfo' method is never supposed to be called" << LOG_END
}
    
void SSHIO::createChannel(const char *remoteCommand, int *protocol)
{
    closeChannel();

    libssh2_session_set_blocking(d_session, 1);

    if (!d_session) {
        LOG_FATAL(SSH_NO_SESSION) << "No SSH session available" << LOG_END
    }

    d_channel = libssh2_channel_open_session(d_session);
    if (!d_channel) {
        LOG_FATAL(SSH_CREATE) << "Failed to create a new ssh channel" << LOG_END
    }

    int rc = libssh2_channel_exec(d_channel, remoteCommand);
    if (rc != 0) {
        LOG_FATAL(SSH_EXEC) << "Failed to execute the remote command '" << remoteCommand
                            << "': " << getLastError() << LOG_END
    }

    libssh2_session_set_blocking(d_session, 0);
    d_recentWrites.clear();
}

void SSHIO::closeChannel()
{
    if (d_channel) {
        libssh2_channel_close(d_channel);
        libssh2_channel_free(d_channel);
        d_channel = 0;
    }
}

void SSHIO::closeSession()
{
    int rc;
    if (d_session) {
        closeChannel();

        int64_t startTime = TimeUtil::getTimeOfDay() / 1000000;
        while ((rc = libssh2_session_disconnect(d_session, "work done")) == LIBSSH2_ERROR_EAGAIN) {
            if (TimeUtil::getTimeOfDay() / 1000000 - startTime > 10) {
                break;
            }
        }

        startTime = TimeUtil::getTimeOfDay() / 1000000;
        while ((rc = libssh2_session_free(d_session)) == LIBSSH2_ERROR_EAGAIN) {
            if (TimeUtil::getTimeOfDay() / 1000000 - startTime > 10) {
                break;
            }
        }
        d_session = 0;
    }

    if (d_socket) {
        SocketUtil::close(d_socket);
        d_socket = 0;
    }
}

int SSHIO::read(char *buffer, int size)
{
    // Make sure the receive window is large 
    const unsigned long minWindowSize = 128 * 1024;
    unsigned long windowSize = libssh2_channel_window_read_ex(d_channel, NULL, NULL);
    if (windowSize < minWindowSize) {
        libssh2_channel_receive_window_adjust2(d_channel, minWindowSize * 2, 0, 0);
    }

    int rc = libssh2_channel_read(d_channel, buffer, size);
    if (rc > 0) {
        return rc;
    }

    checkError(rc);
    return 0;
}

int SSHIO::write(const char *buffer, int size)
{
    int rc = libssh2_channel_write(d_channel, buffer, size);
    if (rc > 0) {
        // Save rc and the first 4 bytes for dump() in case an error is encountered.
        uint64_t value = rc;
        value <<= 32;
        ::memcpy(&value, buffer, (size < 4) ? size : 4);
        if (d_recentWrites.size() > 1000) {
            d_recentWrites.pop_front();
        }
        d_recentWrites.push_back(value);
        return rc;
    }

    checkError(rc);
    return 0;
}

void SSHIO::flush()
{
    libssh2_channel_flush(d_channel);
}

void SSHIO::dump()
{
    const char *hex = "0123456789ABCDEF0";
    for (std::list<uint64_t>::iterator iter = d_recentWrites.begin(); iter != d_recentWrites.end(); ++iter) {
        uint64_t value = *iter;
        int rc = value >> 32;

        char data[9];
        int n = 4;
        if (rc < n) {
            n = rc;
        }
        unsigned char *buffer = reinterpret_cast<unsigned char *>(&value);
        for (int i = 0; i < n ; ++i) {
            unsigned char c = buffer[i];
            data[2 * i] = hex[c / 16];
            data[2 * i + 1] = hex[c % 16];
        }
        data[2 * n] = 0;
        if (n == 4 && buffer[3] == 0x07) {
            int length = (unsigned char)buffer[0] + 256 * (unsigned char)buffer[1] + 65536 * (unsigned char)buffer[2];
            LOG_INFO(SSHIO_DUMP) << "[Debug] Wrote " << rc << " bytes: " << data << "(" << length << ")" << LOG_END
        } else {
            LOG_INFO(SSHIO_DUMP) << "[Debug] Wrote " << rc << " bytes: " << data << LOG_END
        }
    }
}

bool SSHIO::isReadable(int timeoutInMilliSeconds)
{
    return SocketUtil::isReadable(d_socket, timeoutInMilliSeconds);
}

bool SSHIO::isWritable(int timeoutInMilliSeconds)
{
    return SocketUtil::isWritable(d_socket, timeoutInMilliSeconds);
}

void SSHIO::checkError(int rc)
{
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        return;
    }

    if (rc == 0) {
        // Get STDERR messages if there are any.
        char buffer[256];
        std::string message;
        while ((rc = libssh2_channel_read_stderr(d_channel, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[rc] = 0;
            message += buffer;
        }
        if (message.size()) {
            std::string line;
            std::istringstream iss(message);
            while (getline(iss, line, '\n')) {
                LOG_INFO(SSH_STDERR) << line.c_str() << LOG_END
            }
            LOG_FATAL(SSH_CLOSED) << "The ssh channel has been unexpectedly closed" << LOG_END
        }
        return;
    }

    switch (rc) {
    case LIBSSH2_ERROR_ALLOC:
        LOG_FATAL(SSH_ALLOC) << "Allocation failed during an SSH operation" << LOG_END
        break;
    case LIBSSH2_ERROR_SOCKET_SEND:
        LOG_FATAL(SSH_SOCK) << "Unable to send data over the SSH channel" << LOG_END
        break;
    case LIBSSH2_ERROR_CHANNEL_CLOSED:
        LOG_FATAL(SSH_CLOSED) << "The ssh channel has been closed" << LOG_END
        break;
    case LIBSSH2_ERROR_CHANNEL_EOF_SENT:
        LOG_FATAL(SSH_EOF) << "The ssh channel has been requested to be closed" << LOG_END
        break;
    default:
        LOG_FATAL(SSH_ERROR) << "SSH error: " <<  getLastError() << LOG_END
        break;
    }
    return;
}

bool SSHIO::isClosed()
{
    // EOF must be explicitly checked to detect channel closing.
    if (libssh2_channel_eof(d_channel)) {
        LOG_FATAL(SSH_EOF) << "The ssh channel has been closed" << LOG_END
        return true;
    }

    int status = libssh2_channel_get_exit_status(d_channel);
    if (status) {
        LOG_FATAL(SSH_EXIT) << "The remote program has exited with status '" << status << "'" << LOG_END
        return true;
    }

    char *signal = 0;
    libssh2_channel_get_exit_signal(d_channel, &signal, 0, 0, 0, 0, 0);
    if (signal != 0) {
        closeChannel();
        LOG_FATAL(SSH_SIGNAL) << "The channel was closed by signal '" << signal << "'" << LOG_END
        return true;
    }
    return false;
}

std::string SSHIO::getLastError()
{
    char *message;
    int length;

    if (libssh2_session_last_error(d_session, &message, &length, 0)) {
        return std::string(message, length);
    } else {
        return std::string("<no error>");
    }
}

} // namespace rsync
