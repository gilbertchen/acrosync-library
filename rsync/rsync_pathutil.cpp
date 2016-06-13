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

#include <rsync/rsync_pathutil.h>

#include <rsync/rsync_entry.h>
#include <rsync/rsync_log.h>
#include <rsync/rsync_util.h>
#include <rsync/rsync_timeutil.h>

#include <algorithm>

#include <cstring>

#include <qi/qi_build.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

namespace rsync
{

std::string PathUtil::join(const char *top, const char *path)
{
    std::string fullPath = top;
    size_t length = fullPath.size();
    if (length && fullPath[length - 1] != '/' && fullPath[length - 1] != '\\') {
        fullPath += "/";
    }
    fullPath += path;
    return fullPath;
}

std::string PathUtil::getDirectory(const char *fullPath)
{
    const char *pos = strrchr(fullPath, '/');
    if (pos) {
        return std::string(fullPath, pos - fullPath);
    } else {
        return std::string("");
    }
}

std::string PathUtil::getBase(const char *fullPath)
{
    const char *pos = strrchr(fullPath, '/');
    if (pos) {
        if (*(pos + 1) == 0) {
            const char *end = pos;
            --pos;
            if (pos < fullPath) {
                return std::string();
            }
            while (pos > fullPath && *pos != '/') {
                --pos;
            }
            if (*pos == '/') {
                return std::string(pos + 1, end - pos - 1);
            } else {
                return std::string(pos, end - pos);
            }
            
        }
        return std::string(pos + 1);
    } else {
        return std::string(fullPath);
    }
}

bool PathUtil::isPrefix(const char *directory, const char *path)
{
    bool isSeparator = false;
    while (*directory == *path) {
        if (*directory == 0) {
            return true;
        }
        isSeparator = (*directory == '/' || *directory == '\\');
        ++directory;
        ++path;
    }

    if (*directory == 0 && (*path == '/' || isSeparator)) {
        return true;
    }

    return false;
}

bool PathUtil::createIntermediateDirectories(const char *top, const char *dir)
{
    std::string path, fullPath;
    const char *p = dir;
    while (*p != 0) {
        if ((*p == '/' || *p == '\\') && !path.empty()) {
            path += "/";
            fullPath = join(top, path.c_str()); 
            if (!exists(fullPath.c_str())) {
                createDirectory(fullPath.c_str());
            } else {
                if (!isDirectory(fullPath.c_str())) {
                    return false;
                }
            }
        } else {
            path += *p;
        }
        ++p;
    }
    return true;
}

void PathUtil::removeDirectoryRecursively(const char *fullPath)
{
    std::vector<Entry*> files;
    std::vector<Entry*> directories;

    listDirectory(fullPath, "", &files, &directories);
    for (size_t i = 0; i < files.size(); ++i) {
        remove(join(fullPath, files[i]->getPath()).c_str());
        delete files[i];
    }

    for (size_t i = 0; i < directories.size(); ++i) {
        std::string path = join(fullPath, directories[i]->getPath());
        removeDirectoryRecursively(path.substr(0, path.size() - 1).c_str());
        delete directories[i];
    }

    remove(fullPath);
}

void PathUtil::standardizePath(std::string *path, char delimiter)
{
    for (size_t i = 0; i < path->size(); ++i) {
        if ((*path)[i] == '\\' || (*path)[i] == '/') {
            (*path)[i] = delimiter;
        }
    }
}
    
} // close namespace rsync

#if defined(WIN32) || defined(__MINGW32__)

#include <windows.h>
#include <winbase.h>

namespace rsync
{

std::basic_string<wchar_t> PathUtil::convertToUTF16(const char *path)
{
    std::string pathInUTF8(path);
    std::replace(pathInUTF8.begin(), pathInUTF8.end(), '/', '\\');

    int bytes = MultiByteToWideChar(CP_UTF8, 0, pathInUTF8.c_str(), -1, 0, 0);
    wchar_t *buffer = new wchar_t[bytes];
    if (!MultiByteToWideChar(CP_UTF8, 0, pathInUTF8.c_str(), -1, buffer, bytes)) {
        delete [] buffer;
        LOG_FATAL(FILEUTIL_UNICODE) << "Failed to convert the path from UTF8 to UTF16" << LOG_END
    }
    std::basic_string<wchar_t> pathInUTF16(L"\\\\?\\");
    pathInUTF16 += buffer;
    delete [] buffer;
    return pathInUTF16;
}

std::string PathUtil::convertToUTF8(const wchar_t *path)
{
    std::basic_string<wchar_t> pathInUTF16(path);
    if (pathInUTF16.substr(0, 4) == L"\\\\?\\") {
        pathInUTF16 = pathInUTF16.substr(4);
    }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, pathInUTF16.c_str(), -1, 0, 0, NULL, NULL);
    char *buffer = new char[bytes];
    if (!WideCharToMultiByte(CP_UTF8, 0, pathInUTF16.c_str(), -1, buffer, bytes, NULL, NULL)) {
        delete [] buffer;
        LOG_FATAL(FILEUTIL_UNICODE) << "Failed to convert from the path from UTF16 to UTF8" << LOG_END
    }
    std::string pathInUTF8(buffer);
    std::replace(pathInUTF8.begin(), pathInUTF8.end(), '\\', '/');
    delete [] buffer;
    return pathInUTF8;
}

int64_t PathUtil::getSize(const char *fullPath)
{
    std::basic_string<wchar_t> path = convertToUTF16(fullPath);
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileData)) {
        return 0;
    }

    return (int64_t(fileData.nFileSizeHigh) << 32) | fileData.nFileSizeLow;
}

bool PathUtil::exists(const char *fullPath)
{
    std::basic_string<wchar_t> path = convertToUTF16(fullPath);
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool PathUtil::remove(const char *fullPath, bool enableLogging, bool sendToRecycleBin)
{
    std::basic_string<wchar_t> path = convertToUTF16(fullPath);

    // sendToRecyleBin not working due to SHFileOperationW returning 0x2.
    if (0) {

        if (path.substr(0, 4) == L"\\\\?\\") {
            path = path.substr(4);
        }

        SHFILEOPSTRUCTW fileop = { 0 };
        fileop.hwnd = NULL;
        fileop.wFunc = FO_DELETE;
        fileop.pFrom = path.c_str();
        fileop.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NO_UI;
        int error = SHFileOperationW(&fileop);
        if (error) {
            if (enableLogging) {
                LOG_ERROR(FILEUTIL_REMOVE) << "Failed to send file '"<< fullPath << "' to the recycle bin, error code: " << error << LOG_END
            }
            return false;
        } else {
            return true;
        }
    }

    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (!RemoveDirectoryW(path.c_str())) {
            if (enableLogging) {
                LOG_ERROR(FILEUTIL_REMOVE) << "Failed to remove directory '"<< fullPath << "': " << Util::getLastError() << LOG_END
            }
            return false;
        } else {
            return true;
        }
    }
    if (!DeleteFileW(path.c_str())) {
        if (enableLogging) {
            LOG_ERROR(FILEUTIL_REMOVE) << "Failed to remove file '" << fullPath << "': " << Util::getLastError() << LOG_END
        }
        return false;
    } else {
        return true;
    }
}

bool PathUtil::rename(const char *from, const char *to, bool enableLogging)
{
    std::basic_string<wchar_t> fromPath = convertToUTF16(from);
    std::basic_string<wchar_t> toPath = convertToUTF16(to);

    DWORD attribute = GetFileAttributesW(toPath.c_str());
    if (attribute != INVALID_FILE_ATTRIBUTES) {
        if (attribute & FILE_ATTRIBUTE_READONLY) {
            if (!SetFileAttributesW(toPath.c_str(), attribute ^ INVALID_FILE_ATTRIBUTES)) {
                if (enableLogging) {
                    LOG_ERROR(FILEUTIL_RENAME) << "Failed to replace the readonly file '"<< to << "': " << Util::getLastError() << LOG_END
                }
                return false;
            }
        }

        if (!remove(to, enableLogging, true)) {
            return false;
        }
    }

    if (MoveFileW(fromPath.c_str(), toPath.c_str()) == 0) {
        if (enableLogging) {
            LOG_ERROR(FILEUTIL_RENAME) << "Failed to rename '" << from
                                   << "' to '" << to << "': " << Util::getLastError() << LOG_END
        }
        return false;
    }

    if (attribute != INVALID_FILE_ATTRIBUTES && attribute & FILE_ATTRIBUTE_READONLY) {
        if (!SetFileAttributesW(toPath.c_str(), attribute)) {
            if (enableLogging) {
                LOG_ERROR(FILEUTIL_RENAME) << "Failed to reassign the readonly attribute to file '"<< to << "': " << Util::getLastError() << LOG_END
            }
            return false;
        }
    }
    
    return true;
}

bool PathUtil::isDirectory(const char *fullPath)
{
    if (::strlen(fullPath) == 2 && fullPath[1] == ':') {
        return true;
    }
    std::basic_string<wchar_t> path = convertToUTF16(fullPath);
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool PathUtil::createDirectory(const char *fullPath, bool hidden)
{
    std::basic_string<wchar_t> path = convertToUTF16(fullPath);
    if (CreateDirectoryW(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        
        if (hidden) {
            DWORD attributes = GetFileAttributesW(path.c_str());
            if ( attributes == INVALID_FILE_ATTRIBUTES) {
                return false;
            }
            SetFileAttributesW(path.c_str(), attributes | FILE_ATTRIBUTE_HIDDEN);
        }
        return true;
    }

    return false;
}

bool PathUtil::setModifiedTime(const char *fullPath, int64_t time)
{
    std::basic_string<wchar_t> path = convertToUTF16(fullPath);
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE,
                                0, 0, OPEN_EXISTING,
                                FILE_WRITE_ATTRIBUTES, 0);

    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    uint32_t fileTimeHigh, fileTimeLow;
    TimeUtil::getWindowsTime(time, &fileTimeHigh, &fileTimeLow);
    FILETIME fileTime;
    fileTime.dwHighDateTime = fileTimeHigh;
    fileTime.dwLowDateTime = fileTimeLow;
    if (!SetFileTime(handle, 0, 0, &fileTime)) {
        LOG_ERROR(FILEUTIL_SETMTIME) << "Failed to update the modified time of '" << fullPath << "': " << Util::getLastError() << LOG_END
        CloseHandle(handle);
        return false;
    }
    CloseHandle(handle);
    return true;
}


bool PathUtil::setMode(const char *fullPath, uint32_t mode)
{
    if (!(mode & Entry::IS_FILE)) {
        return true;
    }
    DWORD attributes = FILE_ATTRIBUTE_NORMAL;
    if (!(mode & Entry::IS_WRITABLE)) {
        attributes |= FILE_ATTRIBUTE_READONLY;
    }

    std::basic_string<wchar_t> path = convertToUTF16(fullPath);
    if (!SetFileAttributesW(path.c_str(), attributes)) {
        LOG_ERROR(RSYNC_CHMOD) << "Can't chmod '" << fullPath << "': " << Util::getLastError() << LOG_END
        return false;
    }
    return true;
}

namespace {

typedef BOOLEAN (WINAPI* CreateSymbolicLinkW_funct_type)(LPCWSTR, LPCWSTR, DWORD);

CreateSymbolicLinkW_funct_type CreateSymbolicLinkW_funct = reinterpret_cast<CreateSymbolicLinkW_funct_type>(1);

bool enableSymbolicLinkPrivilege()
{
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token) ||
        token == INVALID_HANDLE_VALUE) {
        LOG_DEBUG(RSYNC_SYMLINK) << "Can't obtain the current process token: " << Util::getLastError() << LOG_END
        return false;
    }
    
    LUID luid;
    if (!LookupPrivilegeValue(NULL, "SeCreateSymbolicLinkPrivilege", &luid)) {
        LOG_DEBUG(RSYNC_SYMLINK) << "Failed to look up the symbolic link previlege value: " << Util::getLastError() << LOG_END
        return false;
    }

    TOKEN_PRIVILEGES token_privileges;
    token_privileges.PrivilegeCount                = 1;
    token_privileges.Privileges[0].Luid            = luid;
    token_privileges.Privileges[0].Attributes    = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, NULL, NULL) ||
        GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        LOG_DEBUG(RSYNC_SYMLINK) << "Failed to adjust the token privilege for creating symbolic links: " << Util::getLastError() << LOG_END
        return false;
    }

    return true;
}

} //  close unnamed namespace

bool PathUtil::createSymlink(const char *fullPath, const char *target, bool isDir)
{
    if (CreateSymbolicLinkW_funct == reinterpret_cast<CreateSymbolicLinkW_funct_type>(1)) {
        HMODULE module = GetModuleHandleA ("Kernel32.dll");
        if (Util::isProcessElevated() && module) {
            CreateSymbolicLinkW_funct = (CreateSymbolicLinkW_funct_type)GetProcAddress(module, "CreateSymbolicLinkW");
            if (CreateSymbolicLinkW_funct) {
                if (!enableSymbolicLinkPrivilege()) {
                    LOG_INFO(RSYNC_SYMLINK) << "The user has no previlege to create symbolic links" << LOG_END
                    CreateSymbolicLinkW_funct = 0;
                }
            }
        } else {
            CreateSymbolicLinkW_funct = 0;
        }
    }

    if (CreateSymbolicLinkW_funct == 0) {
        LOG_INFO(RSYNC_SYMLINK) << "Skip the symlink '" << fullPath << "'" << LOG_END
        return false;
    }

    std::basic_string<wchar_t> pathInUTF16 = convertToUTF16(fullPath);
    std::basic_string<wchar_t> targetInUTF16 = convertToUTF16(target);

    if (!CreateSymbolicLinkW_funct(pathInUTF16.c_str(), targetInUTF16.c_str(), isDir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)) {
        LOG_ERROR(RSYNC_SYMLINK) << "Can't create symbol link '" << fullPath << "': " << Util::getLastError() << LOG_END
        return false;
    }

    return true;
}

bool PathUtil::readSymlink(const char *fullPath, std::string *target)
{
    typedef struct _REPARSE_DATA_BUFFER {
      ULONG  ReparseTag;
      USHORT ReparseDataLength;
      USHORT Reserved;
      union {
        struct {
          USHORT SubstituteNameOffset;
          USHORT SubstituteNameLength;
          USHORT PrintNameOffset;
          USHORT PrintNameLength;
          ULONG  Flags;
          WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
          USHORT SubstituteNameOffset;
          USHORT SubstituteNameLength;
          USHORT PrintNameOffset;
          USHORT PrintNameLength;
          WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
          UCHAR DataBuffer[1];
        } GenericReparseBuffer;
      };
    } REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

    std::basic_string<wchar_t> pathInUTF16 = convertToUTF16(fullPath);
    HANDLE handle = CreateFileW(pathInUTF16.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ, 0, OPEN_EXISTING,
                                 FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        LOG_ERROR(RSYNC_REPARSE_POINT) << "Can't open the reparse point '" << fullPath << "': " << Util::getLastError() << LOG_END
        return false;
    }

    char buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
   
    // Make up the control code - see CTL_CODE on ntddk.h
    DWORD controlCode = (FILE_DEVICE_FILE_SYSTEM << 16) | (FILE_ANY_ACCESS << 14) |
                        (FSCTL_GET_REPARSE_POINT << 2) | METHOD_BUFFERED;
    DWORD bytesReturned;
    if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, 0, 0, &buffer, sizeof(buffer), &bytesReturned, 0)) {
        LOG_ERROR(RSYNC_REPARSE_POINT) << "Can't get information about the reparse point '" << fullPath << "': " << Util::getLastError() << LOG_END
        CloseHandle(handle);
        return false;
    }

    REPARSE_DATA_BUFFER *reparseData = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);
    std::basic_string<wchar_t> targetInUTF16;
    if (reparseData->ReparseTag == 0xA000000C) {
        if (reparseData->SymbolicLinkReparseBuffer.PrintNameLength) {
            targetInUTF16.assign(reparseData->SymbolicLinkReparseBuffer.PathBuffer + reparseData->SymbolicLinkReparseBuffer.PrintNameOffset / 2,
                            reparseData->SymbolicLinkReparseBuffer.PrintNameLength / 2);
        } else if (reparseData->SymbolicLinkReparseBuffer.SubstituteNameLength) {
            targetInUTF16.assign(reparseData->SymbolicLinkReparseBuffer.PathBuffer + reparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / 2,
                            reparseData->SymbolicLinkReparseBuffer.SubstituteNameLength / 2);
        } else {
            LOG_ERROR(RSYNC_REPARSE_POINT) << "Can't retrieve the target of symlink '" << fullPath << "': " << Util::getLastError() << LOG_END
            CloseHandle(handle);
            return false;
        }
    } else if (reparseData->ReparseTag == 0xA0000003) {
        if (reparseData->MountPointReparseBuffer.PrintNameLength) {
            targetInUTF16.assign(reparseData->MountPointReparseBuffer.PathBuffer + reparseData->MountPointReparseBuffer.PrintNameOffset / 2,
                reparseData->MountPointReparseBuffer.PrintNameLength / 2);
        }
        else if (reparseData->MountPointReparseBuffer.SubstituteNameLength) {
            targetInUTF16.assign(reparseData->MountPointReparseBuffer.PathBuffer + reparseData->MountPointReparseBuffer.SubstituteNameOffset / 2,
                reparseData->MountPointReparseBuffer.SubstituteNameLength / 2);
        } else {
            LOG_ERROR(RSYNC_REPARSE_POINT) << "Can't retrieve the target of junction '" << fullPath << "': " << Util::getLastError() << LOG_END
                CloseHandle(handle);
            return false;
        }
    } else {
        LOG_ERROR(RSYNC_REPARSE_POINT) << "Skip reparse point that is neither symlink nor junction: " << fullPath << LOG_END
        CloseHandle(handle);
        return false;
    }

    *target = convertToUTF8(targetInUTF16.c_str());
    CloseHandle(handle);
    return true;
}

void PathUtil::listDirectory(const char *top, const char *relativePath, std::vector<Entry*> *fileList,
                             std::vector<Entry*> *directoryList,
                             bool /*normalization*/)
{
    int topLength = ::strlen(top);
    std::string pathInUTF8(top);
    pathInUTF8 += '/';
    if (*relativePath != 0 && ::strcmp(relativePath, "./") != 0) {
        pathInUTF8 += relativePath;
    }

    WIN32_FIND_DATAW fileData;

    std::basic_string<wchar_t> currentPath = convertToUTF16(pathInUTF8.c_str());

    std::basic_string<wchar_t> pattern = currentPath + L"*";

    HANDLE handle = FindFirstFileW(pattern.c_str(), &fileData);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    std::vector<Entry*> currentList;

    do {

        std::basic_string<wchar_t> path = fileData.cFileName;
        if (path[0] == L'.') {
            if (path == L"." || path == L"..") {
                continue;
            }
            if (path == L".acrosync") {
                continue;
            }
            
        }

        std::string fullPath = convertToUTF8((currentPath + path).c_str());

        uint32_t mode = 0;
        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            mode = Entry::IS_LINK | Entry::IS_FILE | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE | Entry::IS_ALL_EXECUTABLE;
        } else if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            mode = Entry::IS_DIR | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE | Entry::IS_ALL_EXECUTABLE;
        } else if (fileData.dwFileAttributes & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) {
            if (fileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                mode = Entry::IS_FILE | Entry::IS_ALL_READABLE;
            } else {
                mode = Entry::IS_FILE | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE;
            }
        }

        Entry *entry = new Entry(fullPath.c_str() + topLength + 1,
                                 (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 0 : fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY,
                                 (int64_t(fileData.nFileSizeHigh) << 32) | fileData.nFileSizeLow,
                                 TimeUtil::getUnixTime(fileData.ftLastWriteTime.dwHighDateTime,
                                                       fileData.ftLastWriteTime.dwLowDateTime),
                                 mode);

        entry->normalizePath();
        currentList.push_back(entry);

    } while(FindNextFileW(handle, &fileData) != 0);

    FindClose(handle);

    std::sort(currentList.begin(), currentList.end(), Entry::compareLocally);

    // First scan the files
    for (size_t i = 0; i < currentList.size(); ++i) {
        if (!currentList[i]->isDirectory()) {
            if (currentList[i]->isLink()) {
                std::string target;
                if (readSymlink(PathUtil::join(top, currentList[i]->getPath()).c_str(), &target)) {
                    currentList[i]->setSymlink(target.c_str());
                } else {
                    delete currentList[i];
                    currentList[i] = 0;
                    continue;
                }
            }
            fileList->push_back(currentList[i]);
            currentList[i] = 0;
        }
    }

    // Next scan the directories in reverse order
    for (size_t i = currentList.size(); i-- > 0; ) {
        if (!currentList[i]) {
            continue;
        }
        if (currentList[i]->isDirectory() && directoryList) {
            directoryList->push_back(currentList[i]);
        } else {
            delete currentList[i];
        }
    }
}

Entry* PathUtil::createEntry(const char *top, const char *path)
{
    std::basic_string<wchar_t> fullPath = convertToUTF16(PathUtil::join(top, path).c_str());
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (!GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fileData)) {
        return 0;
    }

    bool isDirectory = fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    int64_t size = (int64_t(fileData.nFileSizeHigh) << 32) | fileData.nFileSizeLow;
    int64_t time = TimeUtil::getUnixTime(fileData.ftLastWriteTime.dwHighDateTime,
                                         fileData.ftLastWriteTime.dwLowDateTime);

    uint32_t mode = 0;
    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        mode = Entry::IS_DIR | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE | Entry::IS_ALL_EXECUTABLE;
    } else if (fileData.dwFileAttributes & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) {
        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
            mode = Entry::IS_FILE | Entry::IS_ALL_READABLE;
        } else {
            mode = Entry::IS_FILE | Entry::IS_ALL_READABLE | Entry::IS_WRITABLE;
        }
    }

    return new Entry(path, isDirectory, size, time, mode);
}

bool PathUtil::getFileInfo(const char *fullPath, bool *isDir, int64_t *size, int64_t *time)
{
    std::basic_string<wchar_t> pathInUTF16 = convertToUTF16(fullPath);
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (!GetFileAttributesExW(pathInUTF16.c_str(), GetFileExInfoStandard, &fileData)) {
        return false;
    }

    *isDir = fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    *size = (int64_t(fileData.nFileSizeHigh) << 32) | fileData.nFileSizeLow;
    *time = TimeUtil::getUnixTime(fileData.ftLastWriteTime.dwHighDateTime,
                                  fileData.ftLastWriteTime.dwLowDateTime);

    return true;
}


std::string PathUtil::getCurrentDirectory()
{
    int bytes = GetCurrentDirectory(0, NULL);
    wchar_t *buffer = new wchar_t[bytes];
    GetCurrentDirectoryW(bytes, buffer);
    std::string path(convertToUTF8(buffer));
    delete [] buffer;
    return path;
}

} // close namespace rsync

#else // for unix/linux/mac

#include <unistd.h>
#include <dirent.h>
#include <utime.h>

#ifdef __APPLE__
#include <iconv.h>

class UTFConverter
{
public:
    
    static std::string convert (const char *input)
    {
        if (s_iconv_handle == (iconv_t) -1) {
            s_iconv_handle = iconv_open("UTF8", "UTF8-MAC");
            if (s_iconv_handle == (iconv_t) -1) {
                LOG_ERROR(ICONV_FAILURE) << "Failed to convert from UTF8-MAC to UTF8" << LOG_END
                return input;
            }
        }
        
        size_t inleft = strlen(input);
        const char *inbuf = input;

        std::string output;
        
        do {
            size_t outleft = inleft;
            char *buffer = new char[outleft];
            
            char *outbuf = buffer;
            
            errno = 0;
            size_t converted = iconv(s_iconv_handle, (char **) &inbuf, &inleft, &outbuf, &outleft);
            output.append(buffer, outbuf - buffer);
            delete [] buffer;
            if (converted != -1 || errno == EINVAL) {
                // EINVAL: An incomplete multibyte sequence has been encoun­tered in the input.
                // We'll just truncate it and ignore it.
                break;
            }
            
            if (errno != E2BIG) {
                // An invalid multibyte sequence has been encountered in the input.
                // Bad input, we can't really recover from this.
                LOG_ERROR(ICONV_FAILURE) << "Failed to convert from UTF8-MAC to UTF8: invalid multibyte sequence" << LOG_END
                break;
            }
            // E2BIG: There is not sufficient room at *outbuf.
            // We just need to try again.
        } while (1);
        return output;
    }
    
private:
    static iconv_t s_iconv_handle;
};

iconv_t UTFConverter::s_iconv_handle = (iconv_t)-1;

#endif //__APPLE__


namespace rsync
{

int64_t PathUtil::getSize(const char *fullPath)
{
    struct stat buf;
    if (stat(fullPath, &buf) == 0) {
        return buf.st_size;
    } else {
        return 0;
    }
}

bool PathUtil::exists(const char *fullPath)
{
    struct stat buf;
    return stat(fullPath, &buf) == 0;
}

bool PathUtil::remove(const char *fullPath, bool /*enableLogging*/, bool /*sendToRecyleBin*/)
{
    ::remove(fullPath);
    return true;
}

bool PathUtil::rename(const char *from, const char *to, bool /*enableLogging*/)
{
    if (exists(to)) {
        remove(to);
    }
    ::rename(from, to);
    return true;
}

bool PathUtil::isDirectory(const char *fullPath)
{
    struct stat buf;
    if (stat(fullPath, &buf) != 0){
        return false;
    }
    return S_ISDIR(buf.st_mode);
}

bool PathUtil::createDirectory(const char *fullPath, bool /*hidden*/)
{
    return mkdir(fullPath, 0700) == 0;
}

bool PathUtil::setModifiedTime(const char *fullPath, int64_t time)
{
    utimbuf ubuf;
    ubuf.actime = static_cast<time_t>(time);
    ubuf.modtime = static_cast<time_t>(time);
    if (utime(fullPath, &ubuf)) {
        LOG_ERROR(RSYNC_CHANGETIME) << "Can't change modified time for '" << fullPath << "': " << strerror(errno) << LOG_END
        return false;
    } else {
        return true;
    }
}

bool PathUtil::createSymlink(const char *fullPath, const char *link, bool /*isDir*/)
{
    if (symlink(link, fullPath)) {
        LOG_ERROR(RSYNC_SYMLINK) << "Can't create symbol link '" << fullPath << "': " << strerror(errno) << LOG_END
        return false;
    }
    return true;
}

bool PathUtil::setMode(const char *fullPath, uint32_t mode)
{
    if (chmod(fullPath, mode)) {
        LOG_ERROR(RSYNC_CHMOD) << "Can't chmod '" << fullPath << "': " << strerror(errno) << LOG_END
        return false;
    }
    return true;
}

void PathUtil::listDirectory(const char *top, const char *path, std::vector<Entry*> *fileList,
                             std::vector<Entry*> *directoryList, bool normalization)
{
    DIR *dir;
    struct dirent *dirEntry;
    struct stat buf;

    size_t topLength = ::strlen(top);
    std::string currentPath(top);
    currentPath += "/";
    if (*path != 0 && ::strcmp(path, "./") != 0) {
        currentPath += path;
    }
 
    std::vector<Entry*> currentList;

    if ((dir = opendir(currentPath.c_str())) == NULL) {
        return;
    }

    while ((dirEntry = readdir(dir)) != NULL) {
        if (dirEntry->d_name[0] == '.') {
            if (dirEntry->d_name[1] == 0 || (dirEntry->d_name[1] == '.' && dirEntry->d_name[2] == 0)) {
                continue;
            }
            if (::strncmp(dirEntry->d_name + 1, "acrosync", 8) == 0) {
                continue;
            }
        }
        
        bool isDir = (dirEntry->d_type == DT_DIR);
        uint64_t size = 0;
        uint64_t time = 0;
        uint32_t mode = 0;

        std::string file;
        if (normalization) {
#ifdef __APPLE__
            file = currentPath + UTFConverter::convert(dirEntry->d_name);
#else
            file = currentPath + dirEntry->d_name;
#endif
        } else {
            file = currentPath + dirEntry->d_name;
        }

        if (lstat(file.c_str(), &buf) == 0) {
            size = buf.st_size;
            time = buf.st_mtime;
            mode = buf.st_mode;
        }
        
        Entry *entry = new Entry(file.c_str() + topLength + 1,
                                 isDir, size, time, mode);
        
        if (mode & Entry::IS_FILE && mode & Entry::IS_LINK) {
            char buffer[8192];
            ssize_t bytes = ::readlink(file.c_str(), buffer, sizeof(buffer));
            if (bytes > 0) {
                entry->setSymlink(std::string(buffer, bytes));
            } else {
                LOG_INFO(RSYNC_SYMLINK) << "Skip broken symlink '" << file << "'" << LOG_END
                delete entry;
                continue;
            }
        }
        
        entry->normalizePath();
        currentList.push_back(entry);

    }

    closedir(dir);

    std::sort(currentList.begin(), currentList.end(), Entry::compareLocally);

    // First scan the files
    for (unsigned int i = 0; i < currentList.size(); ++i) {
        if (!currentList[i]->isDirectory()) {
           fileList->push_back(currentList[i]);
            currentList[i] = 0;
        }
    }
    
    // Next scan the directories in reverse order
    for (int i = static_cast<int>(currentList.size() - 1); i >= 0; --i) {
        if (!currentList[i]) {
            continue;
        }
        if (currentList[i]->isDirectory() && directoryList) {
           directoryList->push_back(currentList[i]);
        } else {
            delete currentList[i];
        }
    }
}

Entry* PathUtil::createEntry(const char *top, const char *path)
{
    struct stat buf;
    if (stat(PathUtil::join(top, path).c_str(), &buf) != 0) {
        return 0;
    }

    bool isDir = S_ISDIR(buf.st_mode);
    int64_t size = buf.st_size;
    int64_t time = buf.st_mtime;
    uint32_t mode = buf.st_mode;
    return new Entry(path, isDir, size, time, mode);
}

bool PathUtil::getFileInfo(const char *fullPath, bool *isDir, int64_t *size, int64_t *time)
{
    struct stat buf;
    if (stat(fullPath, &buf) != 0) {
        return false;
    }

    *isDir = S_ISDIR(buf.st_mode);
    *size = buf.st_size;
    *time = buf.st_mtime;
    return true;
}

std::string PathUtil::getCurrentDirectory()
{
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) == buffer) {
        return buffer;
    } else {
        return "";
    }
}

} // close namespace rsync

#endif // defined(WIN32) || defined(__MINGW32__)
