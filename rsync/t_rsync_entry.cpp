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

#include <testutil/testutil_assert.h>
#include <testutil/testutil_newdeletemonitor.h>

#include <algorithm>
#include <vector>

#include <string.h>


//qi: TEST_PROGRAM = 1
#include <qi/qi_build.h>

using namespace rsync;

int main(int argc, char *argv[])
{
    TESTUTIL_INIT_RAND;

    const char *unsortedFiles[] = {
        "x",
        "d/c/f",
        "d/",
        "d/e",
        "d/c/",
        "d/c ",
        "ad/",
        "ad/ef",
        "b",
        "f",
    };

    const char *sortedFiles[] = {
        "b",
        "f",
        "x",
        "ad/",
        "ad/ef",
        "d/",
        "d/c ",    
        "d/e",
        "d/c/",
        "d/c/f",
    };

    std::vector<Entry*> list;
    for (int i = 0; i < sizeof(unsortedFiles) / sizeof(unsortedFiles[0]); ++i) {
        Entry *entry =
            new Entry(unsortedFiles[i],
                      unsortedFiles[i][::strlen(unsortedFiles[i]) - 1] == '/', 0, 0, 0);
        list.push_back(entry);
    }

    std::sort(list.begin(), list.end(), Entry::compareGlobally);

    for (unsigned int i = 0; i < list.size(); ++i) {
        //printf("%s\n", list[i]->getPath());
        ASSERT(::strcmp(sortedFiles[i], list[i]->getPath()) == 0);
    }

    for (unsigned int i = 0; i < list.size(); ++i) {
        delete list[i];
    }
    return ASSERT_COUNT;
}
