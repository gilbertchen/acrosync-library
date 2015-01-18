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

#ifndef INCLUDED_RSYNC_FILE_H
#define INCLUDED_RSYNC_FILE_H

#include <stdint.h>

#include <string>

// This class represents a file object to be opened, read, or written.
namespace rsync
{

class File
{
public:

#if defined(WIN32) || defined(__MINGW32__)
    typedef void * Handle;
    static Handle InvalidHandle;

    enum { SEEK_FROM_BEGIN, SEEK_FROM_CURRENT, SEEK_FROM_END };

#else
    typedef int Handle;
    static Handle InvalidHandle;

    enum { SEEK_FROM_BEGIN = SEEK_SET,
           SEEK_FROM_CURRENT = SEEK_CUR,
           SEEK_FROM_END = SEEK_END };
#endif

    // Open a file for read or write.  If an error occurs, report it in log if 'reportError' is true.
    File(const char *fullPath, bool forWrite = false, bool reportError = false);

    // Default constructor creates an invalid file object.
    File();

    // Close the file.
    ~File();

    // Open a file for read or write.  If an error occurs, report it in log if 'reportError' is true.
    bool open(const char *fullPath, bool forWrite, bool reportError);

    // Read 'size' bytes from the file into 'buffer'.
    int read(char *buffer, int size);

    // Write 'size' bytes from 'buffer' into the file.
    int write(const char *buffer, int size);

    // Move the current read/write position.
    int64_t seek(int64_t offset, int method);

    // Close the file
    void close();

    // If an error has occurred.
    bool isValid() const
    {
        return d_handle != InvalidHandle;
    }
 
private:
    // NOT IMPLEMENTED
    File(const File&);
    File operator=(const File&);

    Handle d_handle;
    std::string d_path;
};

} // close namespace rsync

#endif // INCLUDED_RSYNC_FILE_H

