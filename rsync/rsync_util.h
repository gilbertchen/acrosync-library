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

#ifndef INCLUDED_RSYNC_UTIL_H
#define INCLUDED_RSYNC_UTIL_H

#include <rsync/rsync_entry.h>

#include <openssl/md4.h>
#include <openssl/md5.h>

#include <string>
#include <vector>

#include <time.h>

namespace rsync
{

struct Util
{
    // A helpfer class for deleting entries in an entry list.
    class EntryListReleaser
    {
    public:
        EntryListReleaser(std::vector<Entry*> *entryList):
            d_entryList(entryList)
        {
        }

        ~EntryListReleaser()
        {
            for (unsigned int i = 0; i < d_entryList->size(); ++i) {
                delete (*d_entryList)[i];
            }
        }
    private:
        // NOT IMPLEMENTED
        EntryListReleaser(const EntryListReleaser&);
        EntryListReleaser& operator=(const EntryListReleaser&);

        std::vector<Entry*> *d_entryList;
    };

    // For calculating MD4 or MD5 checksums based on the rsync protocol version
    union md_struct
    {
        MD4_CTX md4;
        MD5_CTX md5;
    };
    static void md_init(int protocol, md_struct *context);
    static void md_update(int protocol, md_struct *context, const char *data, int size);
    static void md_final(int protocol, md_struct *context, char digest[16]);

    // For proper intialization and shutdown of dependency libraries.
    static void startup();
    static void cleanup();

    // Return the last error in a readable format.
    static std::string getLastError();

    // Break up a string separated by the delimiters.
    static void tokenize(const std::string line, std::vector<std::string> *results, const char *delimiters);

#if defined(WIN32) || defined(__MINGW32__)

    // If the process has elevated priviledges.
    static bool isProcessElevated();

    // Elevate the process to have a higher priviledge level.
    static bool elevateProcess();
#endif    
};

}

#endif //INCLUDED_RSYNC_UTIL_H
