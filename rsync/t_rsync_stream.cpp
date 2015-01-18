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


#include <rsync/rsync_stream.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include <testutil/testutil_assert.h>
#include <testutil/testutil_newdeletemonitor.h>

//qi: TEST_PROGRAM = 1
#include <qi/qi_build.h>


using namespace rsync;

class StringIO : public IO
{
public:
    StringIO(std::string &data)
        : IO()
        , d_data(data)
        , d_position(0)
    {
    }

    virtual ~StringIO()
    {
    }

    virtual int read(char *buffer, int size)
    {
        int bytes = size;
        if (d_position + size > int(d_data.size())) {
            bytes = d_data.size() - d_position;
        }
        ::memcpy(buffer, d_data.c_str() + d_position, size);
        d_position += bytes;
        return bytes;
    }

    virtual int write(const char *buffer, int size)
    {
        d_data += std::string(buffer, size);
        return size;
    }

    virtual bool isReadable(int timeoutInMilliSeconds)
    {
        return true;
    }

    virtual bool isWritable(int timeoutInMilliSeconds)
    {
        return true;
    }

    virtual bool isClosed()
    {
        return false;
    }

    void reset()
    {
        d_position = 0;
    }

    void createChannel(const char*, int*) {}

    void closeChannel() {}

    void getConnectInfo(std::string*, std::string*, std::string*) {}

    void flush() {}


private:
    // NOT IMPLEMENTED
    StringIO(const StringIO&);
    StringIO& operator=(const StringIO&);

    std::string& d_data;
    int d_position;
};

void testReadWriteVariableInt32()
{
    int32_t DATA[] = {
        0, 1, 127, 128, 129,
        0xffff, 0x10000, 0x10001,
        0xffffff, 0x1000000, 0x1000001,
        0x7ffffffe, 0x7fffffff,
    };

    std::string data;
    StringIO stringIO(data);
    Stream stream(&stringIO);
    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        stream.writeVariableInt32(DATA[i]);
    }

    stringIO.reset();

    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        int32_t n = stream.readVariableInt32();
        ASSERT(n == DATA[i]);
    }
}

void testReadWriteInt64()
{
    int64_t DATA[] = {
        0, 1, 127, 128, 129, 0xffff, 0x10000, 0x10001,
        0xffffff, 0x1000000, 0x1000001,
        0xffffffffll, 0x100000000ll, 0x100000001ll,
        0xffffffffffll, 0x10000000000ll, 0x10000000001ll,
        0xffffffffffffll, 0x1000000000000ll, 0x1000000000001ll,
        0xffffffffffffffll, 0x100000000000000ll, 0x100000000000001ll,
        0x7fffffffffffffffll,
    };

    std::string data;
    StringIO stringIO(data);
    Stream stream(&stringIO);
    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        stream.writeInt64(DATA[i]);
    }

    stringIO.reset();

    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        int64_t n = stream.readInt64();
        ASSERT(n == DATA[i]);
    }
}

void testReadWriteVariableInt64()
{
    int64_t DATA[] = {
        0, 1, 127, 128, 129, 0xffff, 0x10000, 0x10001,
        0xffffff, 0x1000000, 0x1000001,
        0xffffffffll, 0x100000000ll, 0x100000001ll,
        0xffffffffffll, 0x10000000000ll, 0x10000000001ll,
        0xffffffffffffll, 0x1000000000000ll, 0x1000000000001ll,
        0xffffffffffffffll, 0x100000000000000ll, 0x100000000000001ll,
        0x7fffffffffffffffll,
    };

    std::string data;
    StringIO stringIO(data);
    Stream stream(&stringIO);
    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        stream.writeVariableInt64(DATA[i], 3);
    }

    stringIO.reset();

    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        int64_t n = stream.readVariableInt64(3);
        ASSERT(n == DATA[i]);
    }
}

void testReadWriteIndex()
{
    int32_t DATA[] = {
        0, 1, 2, 4, -1, 127, -127,
        0xffff, -0xffff, -0x10000, -0x20000,
        0xffff, 0x10000, 0x20000, 0x100000, 0x200000, 0x1000000,
        20000, 1000, 100, 50, 25
    };

    std::string data;
    StringIO stringIO(data);
    Stream stream(&stringIO);
    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        stream.writeIndex(DATA[i]);
    }

    stringIO.reset();

    for (unsigned int i = 0; i < sizeof(DATA) / sizeof(DATA[0]); ++i) {
        int32_t n = stream.readIndex();
        ASSERT(n == DATA[i]);
    }
}
int main(int argc, char *argv[])
{
    testReadWriteInt64();
    testReadWriteVariableInt32();
    testReadWriteVariableInt64();
    testReadWriteIndex();
    return 0;
}
