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

#ifndef INCLUDED_RSYNC_PATHUTIL_H
#define INCLUDED_RSYNC_PATHUTIL_H

#include <cstdio>

#include <stdint.h>

#include <string>
#include <vector>

namespace rsync
{

class Entry;

struct PathUtil
{

#if defined(WIN32) || defined(__MINGW32__)

    // Convert a UTF8 path to UTF16, adding the "\\?\" prefix.
    static std::basic_string<wchar_t> convertToUTF16(const char *path);

    // Convert a UTF16 path to UTF8, removing the "\\?\" prefix if there is one.
    static std::string convertToUTF8(const wchar_t *path);
#endif

    // Return the file size
    static int64_t getSize(const char *fullPath);

    // Common file/dir operations.
    static bool exists(const char *fullPath);
    static bool remove(const char *fullPath, bool enableLogging = true, bool sendToRecyleBin = false);
    static bool rename(const char *from, const char *to, bool enableLogging = true);
    static bool isDirectory(const char *fullPath);
    static bool createDirectory(const char *fullPath, bool hidden = false);
    static bool readSymlink(const char *fullPath, std::string *target);
    static bool setModifiedTime(const char *fullPath, int64_t time);
    static bool createSymlink(const char *fullPath, const char *target, bool isDir);
    static bool setMode(const char *fullPath, uint32_t mode);

    // Concatenate two paths.
    static std::string join(const char *top, const char *path);

    // Return the directory part of the path
    static std::string getDirectory(const char *fullPath);

    // Return the last component.
    static std::string getBase(const char *fullPath);

    // If 'directory' is a prefix of 'path'
    static bool isPrefix(const char *directory, const char *path);
 
    // Create all intermediate directories between 'top' and 'top/dir'.
    static bool createIntermediateDirectories(const char *top, const char *dir);

    // List the directory joined by 'top' and 'path'; add file entries to 'fileList', and directory entries to
    // 'directoryList'.  If 'includedPatterns' is specified, add only those entries that match the patterns.
    // Files or directories matching any pattern specified in 'excludePatterns' will not be included. 
    // 'normalization' is used to indicate to convert paths from UTF8-MAC to UTF8.
    static void listDirectory(const char *top, const char *path, std::vector<Entry*> *fileList,
                       std::vector<Entry*> *directoryList, bool normalization = false);

    // Create a new entry from the specified path
    static Entry* createEntry(const char *top, const char *path);

    // Return information about a file/directory at the specified path
    static bool getFileInfo(const char *fullPath, bool *isDir, int64_t *size, int64_t *time);

    // Return current working directory
    static std::string getCurrentDirectory();

    // Remove a directory and all its contents, recursively
    static void removeDirectoryRecursively(const char *fullPath);

    // Replace '/' or '\' with the specified delimiter
    static void standardizePath(std::string *path, char delimiter = '/');

};

} // close namespace rsync

#endif // INCLUDED_RSYNC_PATHUTIL_H

