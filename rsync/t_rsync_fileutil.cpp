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
#include <rsync/rsync_file.h>

#include <rsync/rsync_entry.h>

#include <testutil/testutil_assert.h>
#include <testutil/testutil_newdeletemonitor.h>

#include <string.h>

//qi: TEST_PROGRAM = 1
#include <qi/qi_build.h>

#include <openssl/md5.h>

using namespace rsync;

void createFile(const char *top, const char *name, const char *content)
{
    File f(PathUtil::join(top, name).c_str(), true);
    ASSERT(f.isValid());
    int bytes = f.write(content, ::strlen(content));
    ASSERT(bytes > 0);
    ASSERT(bytes == ::strlen(content));
}

std::string getFileChecksum(const char *file)
{
    char buffer[32 * 1024];
   
    MD5_CTX md5;
    MD5_Init(&md5);
 
    File f(file, false);
    int bytes = 0;
    while ((bytes = f.read(buffer, sizeof buffer)) > 0) {
        MD5_Update(&md5, buffer, bytes);
    }

    unsigned char digest[16];
    MD5_Final(digest, &md5);

    return std::string(reinterpret_cast<char *>(&digest[0]), sizeof(digest));
}


int main(int argc, char *argv[])
{
    TESTUTIL_INIT_RAND;

    std::string top = PathUtil::getCurrentDirectory();
    top = PathUtil::join(top.c_str(), "test_dir");

    PathUtil::removeDirectoryRecursively(top.c_str());

    ASSERT(PathUtil::createDirectory(top.c_str()));

    ASSERT(PathUtil::createIntermediateDirectories(top.c_str(), "test_dir1/test_dir2/test_dir3"));
    ASSERT(PathUtil::createDirectory(PathUtil::join(top.c_str(), "test_dir1/test_dir2/test_dir3").c_str()));

    ASSERT(PathUtil::createDirectory(PathUtil::join(top.c_str(), "test_dir4").c_str()));

    const char *content = "this is just a test";
    createFile(top.c_str(), "test_file1", content);

    std::string checksum = getFileChecksum(PathUtil::join(top.c_str(), "test_file1").c_str());

    char buffer[256];
    File f(PathUtil::join(top.c_str(), "test_file1").c_str());
    ASSERT(f.isValid());
    ASSERT(f.read(buffer, 4) == 4);
    ASSERT(::strncmp(buffer, "this", 4) == 0);
    ASSERT(f.read(buffer, 4) == 4);
    ASSERT(::strncmp(buffer, " is ", 4) == 0);
    ASSERT(f.seek(File::SEEK_FROM_BEGIN, 0) == 0);
    ASSERT(f.read(buffer, ::strlen(content)) == ::strlen(content));
    ASSERT(::strncmp(buffer, content, ::strlen(content)) == 0);
    ASSERT(f.read(buffer, ::strlen(content)) == 0);
    f.close();

    createFile(top.c_str(), "test_file2", "this is another test");

    // Call these two 'file' but they are acutally directories
    // to verify the sorting result.
    ASSERT(PathUtil::createDirectory(PathUtil::join(top.c_str(), "test_file4").c_str()));
    ASSERT(PathUtil::createDirectory(PathUtil::join(top.c_str(), "test_file3").c_str()));
     
    std::vector<Entry*> files, directories;

    PathUtil::listDirectory(top.c_str(), "", &files, &directories);

    ASSERT(files.size() == 2);
    ASSERT(::strcmp(files[0]->getPath(), "test_file1") == 0);
    ASSERT(::strcmp(files[1]->getPath(), "test_file2") == 0);

    ASSERT(directories.size() == 4);
    ASSERT(::strcmp(directories[0]->getPath(), "test_file4/") == 0);
    ASSERT(::strcmp(directories[1]->getPath(), "test_file3/") == 0);
    ASSERT(::strcmp(directories[2]->getPath(), "test_dir4/") == 0);
    ASSERT(::strcmp(directories[3]->getPath(), "test_dir1/") == 0);

    for (unsigned int i = 0; i < files.size(); ++i) {
        delete files[i];
    }
    for (unsigned int i = 0; i < directories.size(); ++i) {
        delete directories[i];
    } 
    PathUtil::removeDirectoryRecursively(top.c_str());
    return ASSERT_COUNT;
}
