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

#include <rsync/rsync_util.h>

#include <rsync/rsync_log.h>
#include <rsync/rsync_socketutil.h>
#include <rsync/rsync_sshio.h>

#if defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#else
#include <string.h>
#include <errno.h>
#endif

#include <qi/qi_build.h>

namespace rsync
{

void Util::md_init(int protocol, md_struct *context)
{
    if (protocol >= 30) {
        MD5_Init(&context->md5);
    } else {
        MD4_Init(&context->md4);
    }
}

void Util::md_update(int protocol, md_struct *context, const char *data, int size)
{
    if (protocol >= 30) {
        MD5_Update(&context->md5, data, size);
    } else {
        MD4_Update(&context->md4, data, size);
    }
    
}

void Util::md_final(int protocol, md_struct *context, char digest[16])
{
    if (protocol >= 30) {
        MD5_Final(reinterpret_cast<unsigned char *>(digest), &context->md5);
    } else {
        MD4_Final(reinterpret_cast<unsigned char *>(digest), &context->md4);
    }
    
}

void Util::startup()
{
    rsync::SocketUtil::startup();
    rsync::SSHIO::startup();
}

void Util::cleanup()
{
    rsync::SocketUtil::cleanup();
    rsync::SSHIO::cleanup();
}

std::string Util::getLastError()
{
#if defined(WIN32) || defined(__MINGW32__)
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );
    std::string message((char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
    return message;
#else
    return strerror(errno);
#endif
}

void Util::tokenize(const std::string line, std::vector<std::string> *results, const char *delimiters)
{
    size_t begin = 0;
    while(begin != std::string::npos) {
        size_t end = line.find_first_of(delimiters, begin);
        if (end == std::string::npos) {
            std::string token = line.substr(begin, end);
            if (token.size()) {
                results->push_back(token);
            }
            return;
        }
        if (end != begin) {
            results->push_back(line.substr(begin, end - begin));
        }
        begin = line.find_first_not_of(delimiters, end);
    }
}

#if defined(WIN32) || defined(__MINGW32__)

bool Util::isProcessElevated()
{
    bool isElevated = false;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess( ), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if(GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return isElevated;
}

bool elevateProcess()
{
    // Elevate the process.
    wchar_t szPath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
		return false;
	}

    // Launch itself as administrator.
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = szPath;
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_CANCELLED) {
			return false;
        }
    }
	return true;
}
#endif

} // close namesapce rsysnc

