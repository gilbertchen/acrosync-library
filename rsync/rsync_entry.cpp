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

#include <rsync/rsync_entry.h>

#include <algorithm>

#include <cstring>
#include <cstdio>

#include <qi/qi_build.h>

namespace rsync
{

bool Entry::compareLocally(const Entry* lhs, const Entry* rhs)
{
    // Directories are always arranged before files.  If both are of the same kind, compare them
    // as strings.
    if (lhs->isDirectory()) {
        if (rhs->isDirectory()) {
            return lhs->d_path < rhs->d_path;
        } else {
            return false;
        }
    } else {
        if (rhs->isDirectory()) {
            return true;
        } else {
            return lhs->d_path < rhs->d_path;
        }
    }
}

bool Entry::compareByLocalName(const std::string &lhs, const std::string &rhs)
{
    // Directories are always arranged before files.  If both are of the same kind, compare them
    // as strings.
    if (*lhs.rbegin() == '/') {
        if (*rhs.rbegin() == '/') {
            return lhs < rhs;
        } else {
            return false;
        }
    } else {
        if (*rhs.rbegin() == '/') {
            return true;
        } else {
            return lhs < rhs;
        }
    }
}

bool Entry::compareGlobally(const Entry* lhs, const Entry* rhs)
{
    // If two entries lie within the same directory, compare their local names.  Otherwise, compare their directories.

    const unsigned char *p1 = reinterpret_cast<const unsigned char*>(lhs->getPath());
    const unsigned char *p2 = reinterpret_cast<const unsigned char*>(rhs->getPath());

    while (*p1 == *p2) {
        if (*p1 == 0) {
            return false;
        }
        ++p1;
        ++p2;
    }

    const unsigned char *p3 = p1;
    for (; *p3 != 0 && *p3 != '/'; ++p3) {}
    const unsigned char *p4 = p2;
    for (; *p4 != 0 && *p4 != '/'; ++p4) {}

    if (*p1 < *p2) {
        if (*p1 == 0 && *(p1 - 1) == '/') {
            return true;
        }
        if (*p3 == '/' && *p4 != '/') {
            return false;
        } else {
            return true;
        }
    } else {
        if (*p2 == 0 && *(p2 - 1) == '/') {
            return false;
        }
        if (*p4 == '/' && *p3 != '/') {
            return true;
        } else {
            return false;
        }
    }
}

void Entry::normalizePath()
{
    if (isDirectory() && d_path[d_path.size() - 1] != '/') {
        d_path.append("/");
    }
}
    
bool Entry::contains(const Entry* other) const
{
    return other->d_path.compare(0, d_path.size(), d_path) == 0;
}

bool Entry::isOlderThan(const Entry &other) const
{
#ifdef WIN32
    if (other.d_time > 1426620844) {
        return false;
    }
#endif
    return d_time + 1 < other.d_time;
}

std::string Entry::getLocalName() const
{
    size_t begin = d_path.rfind('/');
    if (begin == d_path.size() - 1) {
        begin = d_path.rfind('/', begin - 1);
    }
    if (begin == std::string::npos) {
        return d_path;
    } else {
        return d_path.substr(begin + 1);
    }
}

} // namespace rsync
