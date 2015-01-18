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

#ifndef INCLUDED_RSYNC_ENTRY_H
#define INCLUDED_RSYNC_ENTRY_H

#include <string>

#include <stdint.h>

#include <sys/stat.h>

namespace rsync
{

// This class encapsulates info about a file/dir, such as file name, file size, modified time, and mode.
class Entry
{
public:

    // Constants for file mode
    enum {
        IS_FILE = 0100000,       /* S_IFREG */
        IS_DIR = 0040000,        /* S_IFDIR */
        IS_READABLE = 0000400,   /* S_IRUSR */
        IS_WRITABLE = 0000200,   /* S_IRUSR */
        IS_EXECUTABLE = 0000100, /* S_IRUSR */
        IS_LINK = 0020000,       /* S_IFLNK */
        IS_ALL_READABLE = 000444,
        IS_ALL_EXECUTABLE = 000111
    };

    // Compare two entries according to their paths, assuming that they are under the same directory.
    static bool compareLocally(const Entry*, const Entry*);

    // Compare two entries according to their full paths.
    static bool compareGlobally(const Entry*, const Entry*);

    // Another version of 'compareLocally' that takes paths rather than entries.
    static bool compareByLocalName(const std::string &lhs, const std::string &rhs);

    // If the entry is a directory, make sure it has the trailing '/'.
    void normalizePath();

    // To determine if 'other' is a file or directory under the directory represented by this entry
    bool contains(const Entry* other) const;

    // Retrun 'true' if this entry is older than 'other'
    bool isOlderThan(const Entry &other) const;

    // Return the last path component
    std::string getLocalName() const;

    // Create a new entry
    Entry(const char *path, bool isDirectory, int64_t size, int64_t time, uint32_t mode)
        : d_path(path)
        , d_size(size)
        , d_time(time)
        , d_mode(mode)
        , d_symlink(0)
        , d_data(0)
    {
        if (isDirectory) {
            d_size = 0;
            d_mode |= IS_DIR;
        }
    }

    ~Entry()
    {
        delete d_symlink;
    }

    Entry(const Entry& other)
        : d_path(other.d_path)
        , d_size(other.d_size)
        , d_time(other.d_time)
        , d_mode(other.d_mode)
        , d_symlink(0)
        , d_data(0)
    {
    }


    const char *getPath() const
    {
        return d_path.c_str();
    }

    bool isDirectory() const
    {
        return (d_mode & IS_DIR) && !(d_mode & IS_FILE);
    }

    void setDirectory()
    {
        d_mode |= IS_DIR;
        d_mode &= ~IS_FILE;
    }

    int64_t getSize() const
    {
        return d_size;
    }

    int64_t getTime() const
    {
        return d_time;
    }

    bool isReadable() const
    {
        return d_mode & IS_READABLE;
    }

    bool isLink() const
    {
        return (d_mode & IS_LINK) && (d_mode & IS_FILE);
    }
    
    bool isRegular() const
    {
        return (d_mode & IS_FILE) && !(d_mode & IS_DIR);
    }
    
    int32_t getMode() const
    {
        return d_mode;
    }
    
    void addSize(int64_t size)
    {
        d_size += size;
    }
    
    void setSymlink(const std::string& symlink)
    {
        delete d_symlink;
        d_symlink = new std::string(symlink);
    }
    
    const char *getSymlink() const
    {
        return d_symlink->c_str();
    }
    
    int getSymlinkLength() const
    {
        return static_cast<int>(d_symlink->size());
    }
    
    void *getData() const
    {
        return d_data;
    }
    
    void setData(void *data) {
        d_data = data;
    }

private:
    // NOT IMPLEMENTED
    Entry& operator=(const Entry&);

    std::string d_path;                  // path of the entry (usually relative to the top directory)
    int64_t d_size;                      // file size; could be 0 for a directory
    int64_t d_time;                      // last modified time
    uint32_t d_mode;                     // file node
    std::string *d_symlink;              // contains the symbolic link; is NULL if the entry is not a symlink

    void *d_data;                        // A generic field usually used to store a pointer
};

} // namespace rsync
#endif //INCLUDED_RSYNC_ENTRY_H
