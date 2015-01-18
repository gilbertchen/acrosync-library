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

#include <rsync/rsync_file.h>

#include <rsync/rsync_log.h>
#include <rsync/rsync_pathutil.h>
#include <rsync/rsync_util.h>

#include <qi/qi_build.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#if defined(WIN32) || defined(__MINGW32__)

#include <Windows.h>

namespace rsync
{

File::Handle File::InvalidHandle = INVALID_HANDLE_VALUE;

File::File()
    : d_handle(InvalidHandle)
    , d_path("")
{
}

File::File(const char *fullPath, bool forWrite, bool reportError)
    : d_handle(InvalidHandle)
    , d_path(fullPath)
{
    open(fullPath, forWrite, reportError);
}

bool File::open(const char *fullPath, bool forWrite, bool reportError)
{
    d_path = fullPath;
    std::basic_string<wchar_t> path = PathUtil::convertToUTF16(fullPath);
    d_handle = CreateFileW(path.c_str(), forWrite ? GENERIC_WRITE : GENERIC_READ,
                           FILE_SHARE_READ, 0, forWrite ? CREATE_ALWAYS : OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, 0);
    if (d_handle == InvalidHandle) {
        if (reportError) {
            LOG_ERROR(FILE_OPEN) << "Failed to open '" << fullPath << "': "
                                 << Util::getLastError() << LOG_END
        }
        return false;
    }
    return true;
}

File::~File()
{
    this->close();
}


int File::read(char *buffer, int size)
{
    if (d_handle == InvalidHandle) {
        return 0;
    }
    DWORD bytes = 0;
    if (!ReadFile(d_handle, buffer, size, &bytes, NULL)) {
        LOG_ERROR(FILE_READ) << "Failed to read " << size << " bytes from '" << d_path << "': "
                             << Util::getLastError() << LOG_END
        this->close();
    }
    return bytes;
}

int File::write(const char *buffer, int size)
{
    if (d_handle == InvalidHandle) {
        return 0;
    }
    DWORD bytes = 0;
    if (!WriteFile(d_handle, buffer, size, &bytes, NULL)) {
        LOG_ERROR(FILE_WRITE) << "Failed to write " << size << " bytes to '" << d_path << "': "
                              << Util::getLastError() << LOG_END
        this->close();
    }
    return bytes;
}

int64_t File::seek(int64_t offset, int method)
{
    if (d_handle == InvalidHandle) {
        return false;
    }
    
    int moveMethods[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
    LONG distanceToMoveHigh = offset >> 32;

    int64_t result = SetFilePointer(d_handle, offset & 0xffffffff, &distanceToMoveHigh, moveMethods[method]);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        LOG_ERROR(FILE_SEEK) << "Failed to seek " << offset << " bytes by method " << method
                             << " in '" << d_path << "': " << Util::getLastError() << LOG_END
        this->close();
        return 0;
    }
    return result | (static_cast<int64_t>(distanceToMoveHigh) << 32);
}

void File::close()
{
    if (d_handle == InvalidHandle) {
        return;
    }
    CloseHandle(d_handle);
    d_handle = InvalidHandle;
}

} // close namespace rsync

#else // for unix/linux/mac

#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <errno.h>
#include <string.h>

namespace rsync
{

File::Handle File::InvalidHandle = -1;

File::File()
    : d_handle(InvalidHandle)
    , d_path("")
{
}

File::File(const char *fullPath, bool forWrite, bool reportError)
    : d_handle(InvalidHandle)
    , d_path(fullPath)
{
    open(fullPath, forWrite, reportError);
}

bool File::open(const char *fullPath, bool forWrite, bool reportError)
{
    d_path = fullPath;
    if (forWrite) {
        d_handle = ::open(fullPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    } else {
        d_handle = ::open(fullPath, O_RDONLY);
    }
    
    if (d_handle == InvalidHandle && reportError) {
        LOG_ERROR(RSYNC_OPEN) << "Failed to open '" << fullPath << "': " << strerror(errno) << LOG_END
    }
    return d_handle != InvalidHandle;
}

File::~File()
{
    this->close();
}

int File::read(char *buffer, int size)
{
    if (d_handle == InvalidHandle) {
        return 0;
    }
    int rc = static_cast<int>(::read(d_handle, buffer, size));
    if (rc == -1) {
        LOG_ERROR(RSYNC_FILE) << "Error reading from '" << d_path << "': " << strerror(errno) << LOG_END
        return 0;
    }
    return rc;
}

int File::write(const char *buffer, int size)
{
    if (d_handle == InvalidHandle) {
        return 0;
    }
    int bytes = 0;
    while (bytes < size) {
        int rc = static_cast<int>(::write(d_handle, buffer + bytes, size - bytes));
        if (rc == 0) {
            LOG_ERROR(RSYNC_FILE) << "Failed to write to '" << d_path << "'" << LOG_END
            return bytes;
        } else if (rc == -1) {
            LOG_ERROR(RSYNC_FILE) << "Error writing '" << d_path << "': " << strerror(errno) << LOG_END
            return bytes;
        }
        bytes += rc;
    }
    return bytes;
}

int64_t File::seek(int64_t offset, int method)
{
    if (d_handle == InvalidHandle) {
        return -1;
    }
#ifdef __APPLE__
    return ::lseek(d_handle, offset, method);
#else
    return ::lseek64(d_handle, offset, method);
#endif
}

void File::close()
{
    if (d_handle == InvalidHandle) {
        return;
    }
    ::close(d_handle);
}

} // close namespace rsync

#endif // defined(WIN32) || defined(__MINGW32__)
