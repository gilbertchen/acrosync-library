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

#include <rsync/rsync_log.h>
#include <rsync/rsync_timeutil.h>
#include <rsync/rsync_util.h>

#include <sstream>

#include <cstring>
#include <cstdio>
#include <ctime>


#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/md4.h>

#include <qi/qi_build.h>

namespace rsync
{

namespace
{

// Return the length of the variable-length integer indicated by 'c'
int getVariableLength(uint8_t c)
{
    for (int i = 0; i <= 6; ++i) {
        if ((c & 0x80) == 0) {
            return i;
        }
        c = c << 1;
    }
    return 6;
}

// Default read/write buffer size
int g_BufferSize = 64000;

// Message flags.  MSG_DATA is followed by data that will be put in the read/write buffer.  Messages with
// other flags are processed differently.
enum { MSG_BASE = 7 };
enum {
    MSG_DATA = 0,
    MSG_ERROR_XFER = 1,
    MSG_INFO = 2,
    MSG_ERROR = 3,
    MSG_WARNING = 4,
    MSG_IO_ERROR = 22,
    MSG_NOOP = 42,
    MSG_SUCCESS = 100,
    MSG_DELETED = 101,
    MSG_NO_SEND = 102
};

// Replace new line characters by spaces.
void stripNewLine(char *message)
{
    for (char *i = message; *i; ++i) {
        if (*i == '\n') {
            *i = ' ';
        }
    }
}

// Encode a string in base64.
std::string base64_encode(char *data, int length)
{
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, data, length);
    BIO_flush(b64);
    
    BUF_MEM *bptr = NULL;
    BIO_get_mem_ptr(b64, &bptr);
    
    char *buff = static_cast<char*>(malloc(bptr->length));
    memset(buff, 0, sizeof(char*));
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = '\0';
    
    std::string encoded(buff);
    
    BIO_free_all(b64);
    free(buff);
    return encoded;
}
    
} // unnamed namespace

Stream::Stream(IO* io, int *cancelFlag)
    : d_io(io)
    , d_cancelFlag(cancelFlag)
    , d_isReadBuffered(false)
    , d_isWriteBuffered(false)
    , d_isWriteMultiplexed(false)
    , d_readDataLength(0)
    , d_writeBuffer(new char[g_BufferSize])
    , d_writeBufferSize(g_BufferSize)
    , d_writeBufferPosition(0)
    , d_readBuffer(new char[g_BufferSize])
    , d_readBufferSize(g_BufferSize)
    , d_readBufferPosition(0)
    , d_readBufferEnd(0)
    , d_flushStart(0)
    , d_automaticFlush(true)
    , d_lastPositiveIndexRead(-1)
    , d_lastNegativeIndexRead(1)
    , d_lastPositiveIndexWritten(-1)
    , d_lastNegativeIndexWritten(1)
    , d_blockedTime(0)
    , d_uploadLimit(0)
    , d_bucketStartTime(0)
    , d_uploadBuckets()
    , d_deletedFiles()
    , d_messageFlag(-1)
{
}

Stream::~Stream()
{
    delete [] d_writeBuffer;
    delete [] d_readBuffer;
}

void Stream::reset()
{
    d_isReadBuffered = false;
    d_isWriteBuffered = false;
    d_isWriteMultiplexed = false;
    d_readDataLength = 0;
    d_readBufferPosition = 0;
    d_writeBufferPosition = 0;
    d_readBufferEnd = 0;
    d_automaticFlush = true;
    d_flushStart = 0;
    d_lastPositiveIndexRead = -1;
    d_lastNegativeIndexRead = 1;
    d_lastPositiveIndexWritten = -1;
    d_lastNegativeIndexWritten = 1;
}

int Stream::read(char *buffer, int size)
{
    // Read directly from the io if the buffer is not being used.
    if (!d_isReadBuffered) {
        return readAll(buffer, size);
    }

    int bytes = 0;

    // Read from the read buffer if there is anything.
    if (d_readBufferEnd > d_readBufferPosition) {
        int readLength = size;
        if (readLength > d_readBufferEnd - d_readBufferPosition) {
            readLength = d_readBufferEnd - d_readBufferPosition;
        }
        ::memcpy(buffer, d_readBuffer + d_readBufferPosition, readLength);
        bytes += readLength;
        d_readBufferPosition += readLength;
        if (d_readBufferPosition >= d_readBufferEnd) {
            d_readBufferPosition = 0;
            d_readBufferEnd = 0;
        }
    }

    // The read buffer is exhausted.  Try to read from the io, which is multiplexed.
    while (bytes < size) {
        d_blockedTime = 0;
        // 'd_readDataLength == 0' means that we've finished reading the current chunk.
        // Need to get a new chunk
        while (d_readDataLength <= 0) {
            uint32_t flag;
            // Read the message flag.  If the message flag is MSG_DATA then d_readDataLength will indicate
            // how many bytes of data are available.
            if (readMessageFlag(&flag)) {
                readMessageContent(flag);
                d_blockedTime = 0;
            } else {
                // Check timeout
                timedWait(true, "read");
            }
        }
        int n = size - bytes;
        if (d_readDataLength < n) {
            n = d_readDataLength;
        }
        // Now we can read from the io directly.
        bytes += readAll(buffer + bytes, n);
        d_readDataLength -= n;
    }
    return size;
}

int Stream::write(const char *buffer, int size)
{
    // Write directly to the io is the buffer mode isn't on.
    if (!d_isWriteBuffered) {
        return writeAll(buffer, size);
    }

    // A non-zero 'd_flushStart' means we started but haven't finished flushing.  In this case a new write is prohibited.
    if (d_flushStart) {
        LOG_FATAL(RSYNC_WRITE) << "Attempt to write while a previous flush operation is incomplete" << LOG_END
        return 0;
    }

    if (d_automaticFlush) {
        if (size < d_writeBufferSize - d_writeBufferPosition) {
            ::memcpy(d_writeBuffer + d_writeBufferPosition, buffer, size);
            d_writeBufferPosition += size;
            return size;
        }
        // Not sufficient space in the write buffer.  Flush it.
        flushWriteBuffer(buffer, size);
    } else {
        // We can't flush.  Grow the write buffer if needed.
        if (size > d_writeBufferSize - d_writeBufferPosition) {
            d_writeBufferSize = d_writeBufferSize * 2;
            if (size + d_writeBufferPosition > d_writeBufferSize) {
                d_writeBufferSize = (size + d_writeBufferPosition) * 2;
            }
            char *newBuffer = new char[d_writeBufferSize];
            ::memcpy(newBuffer, d_writeBuffer, d_writeBufferPosition);
            delete [] d_writeBuffer;
            d_writeBuffer = newBuffer;
        }
        ::memcpy(d_writeBuffer + d_writeBufferPosition, buffer, size);
        d_writeBufferPosition += size;
    }

    return size;
}

void Stream::enableBuffer()
{
    d_isReadBuffered = true;
    d_isWriteBuffered = true;
}

void Stream::enableWriteMultiplex()
{
    d_isWriteMultiplexed = true;
}

bool Stream::readMessageFlag(uint32_t *flag)
{
    // If there is some data available for reading, we can't read the message flag.  Instead, we should read the
    // rest of the current chunk into the read buffer.
    if (d_readDataLength) {
        if (d_readBufferEnd + d_readDataLength > d_readBufferSize) {
            // Grow the read buffer if needed.
            int newSize = d_readBufferSize * 2;
            if (newSize < d_readBufferEnd + d_readDataLength) {
                newSize = (d_readBufferEnd + d_readDataLength) * 2;
            }
            char *newBuffer = new char[newSize];
            ::memcpy(newBuffer, d_readBuffer, d_readBufferEnd);
            delete [] d_readBuffer;
            d_readBuffer = newBuffer;
            d_readBufferSize = newSize;
        }

        int bytes = d_io->read(d_readBuffer + d_readBufferEnd, d_readDataLength);
        d_readBufferEnd += bytes;
        d_readDataLength -= bytes;
        return false;
    }

    // If nothing is available, return immediately; otherwise make sure the flag is read completely.
    int bytes = 0;
    bytes = d_io->read(reinterpret_cast<char *>(flag), sizeof(*flag));

    if (bytes == 0) {
        return false;
    }

    if (bytes < static_cast<int>(sizeof(*flag))) {
        readAll(reinterpret_cast<char *>(flag) + bytes, sizeof(*flag) - bytes);
    }

    return true;
}

void Stream::readMessageContent(uint32_t flag)
{
    int length = flag & 0xffffff;
    flag = (flag >> 24) - MSG_BASE;
    if (flag == MSG_DATA) {
        d_readDataLength = length;
        return;
    }

    d_messageFlag = flag;

    char *message = new char[length + 1];
    message[length] = 0;
    switch (flag) {
    case MSG_NOOP:
        if (length != 0) {
            LOG_FATAL(RSYNC_MSG) << "Invalid noop message" << LOG_END
        }
        break;
    case MSG_IO_ERROR:
        if (length != 4) {
            LOG_FATAL(RSYNC_MSG) << "Invalid io error message" << LOG_END
        }
        readAll(message, length);
        LOG_FATAL(RSYNC_IOERROR) << "Remote I/O error" << LOG_END
        break; 
    case MSG_SUCCESS:
        if (length != 4) {
            LOG_FATAL(RSYNC_MSG) << "Invalid success message" << LOG_END
        }
        readAll(message, length);
        LOG_INFO(RSYNC_SUCESS) << "Remote operator successful" << LOG_END
        break; 
    case MSG_NO_SEND:
        if (length != 4) {
            LOG_FATAL(RSYNC_MSG) << "Invalid no_send message:" << flag << LOG_END
        }
        readAll(message, length);
        LOG_INFO(RSYNC_NOSEND) << "Remote no_send message received" << LOG_END
        break;
    case MSG_INFO:
        readAll(message, length);
        stripNewLine(message);
        LOG_DEBUG(RSYNC_INFO) << "INFO: " << message << LOG_END
        break;
    case MSG_ERROR:
    case MSG_ERROR_XFER:
        readAll(message, length);
        stripNewLine(message);
        LOG_FATAL(RSYNC_ERROR) << message << LOG_END
        break;
    case MSG_WARNING:
        readAll(message, length);
        stripNewLine(message);
        LOG_FATAL(RSYNC_WARNING) << message << LOG_END
        break;
    case MSG_DELETED:
        readAll(message, length);
        stripNewLine(message);
        LOG_DEBUG(RSYNC_DELETE) << message << " has been deleted" << LOG_END
        d_deletedFiles.push_back(message);
        break;
    default:
        LOG_FATAL(RSYNC_FLAG) << "Unexpected flag: " << flag << " (" 
            << d_messageFlag << ":"
            << d_isReadBuffered << ":"
            << d_isWriteBuffered << ":"
            << d_isWriteMultiplexed << ":"
            << d_readDataLength << ":"
            << d_writeBufferSize << ":"
            << d_writeBufferPosition << ":"
            << d_readBufferSize << ":"
            << d_readBufferPosition << ":"
            << d_readBufferEnd << ":"
            << d_flushStart << ":"
            << d_automaticFlush << ")"
            << LOG_END
        break;
    }
    d_messageFlag = -1;
    return;
}

bool Stream::isDataAvailable()
{
    if (d_readDataLength > 0) {
        return true;
    }

    uint32_t flag;
    if (readMessageFlag(&flag)) {
        readMessageContent(flag);
    }
    return d_readDataLength > 0;
}

// Log into the rsync daemon.  Not used in the SSH mode.
// 
// TODO: This method probably shouldn't belong to this class.  Find a better place.
void Stream::login(const char *remoteCommand, int *protocol, std::vector<std::string> *moduleListing)
{
    std::string username;
    std::string password;
    std::string module;
    
    d_io->getConnectInfo(&username, &password, &module);
    
    std::stringstream output;
    output << "@RSYNCD: " << *protocol << ".0\n";
    writeLine(output.str(), true);
    writeLine(std::string(module), true);

    std::string line = readLine();
    if (line.compare(0, 9, "@RSYNCD: ") != 0) {
        LOG_FATAL(RSYNCD_GREETING) << "Greeting not received from the daemon" << LOG_END
        return;
    }
    
    int remoteProtocol = atoi(line.c_str() + 9);
    if (remoteProtocol >= 31) {
        *protocol = 30;
    } else if (remoteProtocol == 29 || remoteProtocol == 30) {
        *protocol = remoteProtocol;
    } else if (remoteProtocol > 0) {
        LOG_FATAL(RSYNCD_PROTOCOL) << "Incompatible protocol " << remoteProtocol << LOG_END
        return;
    } else {
        LOG_FATAL(RSYNCD_GREETING) << "Greeting not received from the daemon" << LOG_END
        return;
    }
    
    while (true) { 
        line = readLine();
        if (line.compare(0, 18, "@RSYNCD: AUTHREQD ") == 0) {
            Util::md_struct mdContext;
            Util::md_init(*protocol, &mdContext);
            Util::md_update(*protocol, &mdContext, password.c_str(), password.size());
            std::string challenge = line.substr(18);
            Util::md_update(*protocol, &mdContext, challenge.c_str(), challenge.size());
            char digest[16];
            Util::md_final(*protocol, &mdContext, digest);
            std::string response = base64_encode(digest, sizeof(digest));
            while (response.back() == '=') {
                response.erase(response.size() - 1);
            }
            output.str("");
            output.clear();
            output << username << " " << response;
            writeLine(output.str(), true);
        } else if (line.compare(0, 11, "@RSYNCD: OK") == 0) {
            
            // We need to break up the remote command to extract command line arguments.
            std::string command(remoteCommand);
            size_t begin = 0;
            std::vector<std::string> args;
            
            while(begin != std::string::npos) {
                if (command[begin] == '"' && command.find_first_of('"', begin + 1)) {
                    size_t end = command.find_first_of('"', begin + 1);
                    args.push_back(command.substr(begin + 1, end - begin - 1));
                    begin = command.find_first_not_of(" ", end + 1);
                } else {
                    size_t end = command.find_first_of(' ', begin + 1);
                    if (end != std::string::npos) {
                        args.push_back(command.substr(begin, end - begin));
                    } else {
                        args.push_back(command.substr(begin));
                    }
                    begin = command.find_first_not_of(" ", end);
                }
            }
            
            // Starting from 1 as the rsync executable path is not needed.
            for (unsigned int i = 1; i < args.size(); ++i) {
                writeLine(args[i], *protocol == 29);
            }
            writeLine(std::string(""), *protocol == 29);
            
            return;
        } else if (line.compare(0, 8, "@ERROR: ") == 0) {
            LOG_FATAL(RSYNCD_ERROR) << "Failed to establish the connection: " << line.c_str() + 8 << LOG_END
            return;
        } else if (line.compare(0, 13, "@RSYNCD: EXIT") == 0) {
            return;
        } else {
            if (moduleListing) {
                moduleListing->push_back(line);
            }
        }
    }
}

void Stream::flushWriteBuffer(const char *additionalData, int additionalDataLength)
{
    if (!d_isWriteBuffered || d_writeBufferPosition == 0) {
        return;
    }

    if (d_flushStart > 0) {
        LOG_FATAL(RSYNC_WRITE) << "Attempt to flush while a previous flush operation is incomplete" << LOG_END
    }

    int length = d_writeBufferPosition + additionalDataLength;
    if (d_isWriteMultiplexed) {
        uint32_t flag = ((MSG_DATA + MSG_BASE) << 24) | (length & 0xffffff);
        writeAll(reinterpret_cast<char *>(&flag), sizeof(flag));
    }
    writeAll(d_writeBuffer, d_writeBufferPosition);
    d_writeBufferPosition = 0;
    if (additionalDataLength) {
        writeAll(additionalData, additionalDataLength);
    }
    d_writeBufferPosition = 0;
    d_flushStart = 0;
}

bool Stream::tryFlushWriteBuffer()
{
    if (d_writeBufferPosition == 0) {
        return true;
    }

    // 'd_flushStart' must take into account the size of flag (which is 4).

    int bytes = 0;
    int flagLength = d_isWriteMultiplexed ? 4 : 0;
    if (d_flushStart < flagLength) {
        uint32_t flag = ((MSG_DATA + MSG_BASE) << 24) | (d_writeBufferPosition & 0xffffff);
        bytes = d_io->write(reinterpret_cast<char *>(&flag) + d_flushStart, sizeof(flag) - d_flushStart);
    } else {
        bytes = d_io->write(d_writeBuffer + d_flushStart - flagLength, d_writeBufferPosition - (d_flushStart - flagLength));
    }

    d_flushStart += bytes;

    if (d_flushStart >= d_writeBufferPosition + flagLength) {
        d_writeBufferPosition = 0;
        d_flushStart = 0;
        return true;
    } else {
        return false;
    }
}

void Stream::disableAutomaticFlush()
{
    if (d_writeBufferPosition > 0) {
        flushWriteBuffer();
    }
    d_automaticFlush = false;
}

int Stream::readAll(char *buffer, int size)
{
    int bytes = 0;
    d_blockedTime = 0;

    while (bytes < size) {
        int rc = d_io->read(buffer + bytes, size - bytes);
        bytes += rc;
        if (rc == 0) {
            timedWait(true, "readAll");
        } else {
            d_blockedTime = 0;
        }
    }

    //LOG_INFO(STREAM_DEBUG) << "ReadALL: " << toHex(buffer, size) << LOG_END

    return size;
}

int Stream::writeAll(const char *buffer, int size)
{
    //LOG_INFO(STREAM_DEBUG) << "Write: " << toHex(buffer, size) << LOG_END

    int bytes = 0;
    d_blockedTime = 0;

    if (d_uploadLimit > 0) {
        // Calculate the current upload speed.  Introduct a delay if we're sending too fast.
        int n = 5;
        int64_t now = TimeUtil::getTimeOfDay();
        if (d_bucketStartTime == 0) {
            d_bucketStartTime = now;
            d_uploadBuckets.push_back(0);
        } else {
            while (now - d_bucketStartTime >= n * 1000000) {
                d_uploadBuckets.erase(d_uploadBuckets.begin());
                d_uploadBuckets.push_back(0);
                d_bucketStartTime += 1000000;
            }
            while (now - d_bucketStartTime >= d_uploadBuckets.size() * 1000000) {
                d_uploadBuckets.push_back(0);
            }
        }

        double sum = 0.0;
        for (unsigned int i = 0; i < d_uploadBuckets.size(); ++i) {
            sum += d_uploadBuckets[i];
        }
        sum += size / 1024;

        if (sum / d_uploadBuckets.size() > d_uploadLimit) {
            
            //    sum + size / 1024
            //   --------------------- == uploadLimit
            //    now + delay - start 
            
            int delay = ((sum / d_uploadLimit) - (now - d_bucketStartTime) / 1000000.0) * 1000.0;
            if (delay > 0) {
                if (delay > 1000) {
                    delay = 1000;
                }
                TimeUtil::sleep(delay);
            }
        }

        d_uploadBuckets[d_uploadBuckets.size() - 1] += size / 1024;

    }

    while (bytes < size) {
        int rc = d_io->write(buffer + bytes, size - bytes);
        bytes += rc;

        if (rc == 0) {
            timedWait(false, "writeAll");

            uint32_t flag;
            if (readMessageFlag(&flag)) {
                readMessageContent(flag);
            }
        } else {
            d_blockedTime = 0;
        }
    }

    return size;
}
 
void Stream::flush()
{
    d_io->flush();
}

void Stream::timedWait(bool isReading, const char *location)
{
    checkCancelFlag();

    const int maximumTimeout = 600;  // in seconds
    bool result;
    if (isReading) {
        result = d_io->isReadable(100); // milliseconds
    } else {
        result = d_io->isWritable(100); // milliseconds
    }

    if (result) {
        d_blockedTime = 0;
    } else {
        if (d_blockedTime == 0) {
            d_blockedTime = ::time(0);
        } else if ((::time(0) - d_blockedTime) > maximumTimeout / 2) {
            d_io->flush();
            if ((::time(0) - d_blockedTime) > maximumTimeout) {
                if (!d_io->isClosed()) {
                    LOG_FATAL(STREAM_TIMEOUT) << "The rsync channel has been blocked for more than "
                                            << maximumTimeout << " seconds ("
                                            << location << ":"
                                            << d_messageFlag << ":"
                                            << d_isReadBuffered << ":"
                                            << d_isWriteBuffered << ":"
                                            << d_isWriteMultiplexed << ":"
                                            << d_readDataLength << ":"
                                            << d_writeBufferSize << ":"
                                            << d_writeBufferPosition << ":"
                                            << d_readBufferSize << ":"
                                            << d_readBufferPosition << ":"
                                            << d_readBufferEnd << ":"
                                            << d_flushStart << ":"
                                            << d_automaticFlush << ")"
                                             << LOG_END
                }
            }
        }
    }
}
    
void Stream::checkCancelFlag() const
{
    if (d_cancelFlag && *d_cancelFlag) {
        LOG_FATAL(RSYNC_CANCEL) << "The operation was cancelled by user" << LOG_END        
    }
}

void Stream::setUploadLimit(int uploadLimit)
{
    d_uploadLimit = uploadLimit;
}

std::string Stream::toHex(const char *buffer, int size)
{
    const char *hex = "O123456789ABCDEF";
    std::string str;
    for (int i = 0; i < size; ++i) {
        unsigned char c = buffer[i];
        str += hex[c / 16];
        str += hex[c % 16];
    }
    return str;
}

uint8_t Stream::readUInt8()
{
    uint8_t i;
    read(reinterpret_cast<char*>(&i), 1);
    return i;
}

uint16_t Stream::readUInt16()
{
    uint16_t i;
    read(reinterpret_cast<char*>(&i), 2);
    return i;
}

int32_t Stream::readInt32()
{
    uint32_t i;
    read(reinterpret_cast<char*>(&i), 4);
    return i;
}

int64_t Stream::readInt64()
{
    int32_t i = readInt32();

    if (i != -1) {
        return i;
    }

    uint32_t low = readInt32();
    uint32_t high = readInt32();
    return static_cast<int64_t>(low) | (static_cast<int64_t>(high) << 32);
}

int32_t Stream::readVariableInt32()
{
    uint8_t firstByte = readUInt8();
    int length = getVariableLength(firstByte);
    if (length == 0) {
        return firstByte;
    } else if (length == 4) {
        int32_t i;
        read(reinterpret_cast<char *>(&i), sizeof(i));
        return i;
    } else if (length > 0 && length < 4) {
        int32_t i = 0;
        char *buffer = reinterpret_cast<char *>(&i);
        read(buffer, length);
        uint8_t mask = (1 << (8 - length)) - 1;
        buffer[length] = firstByte & mask;
        return i;
    } else {
        LOG_FATAL(STREAM_INT32) << "32 bit interger overflow: "
                                << toHex(reinterpret_cast<char*>(&firstByte), 1)
                                << LOG_END
        return 0;
    }
}

int64_t Stream::readVariableInt64(int minimumBytes)
{
    char bytes[9];
    ::memset(bytes, 0, sizeof(bytes));
    read(bytes, minimumBytes);
    int extra = getVariableLength(bytes[0]);
    if (extra + minimumBytes > 9) {
        LOG_FATAL(STREAM_INT64) << "64 bit interger overflow: "
                                << toHex(bytes, minimumBytes)
                                << LOG_END
    }
    if (!extra) {
        bytes[minimumBytes] = bytes[0];
    } else {
        read(bytes + minimumBytes, extra);
        if (extra + minimumBytes < 9) {
            char mask = (1 << (8 - extra)) - 1;
            bytes[extra + minimumBytes] = bytes[0] & mask;
        }
    }
    ::memcpy(bytes, bytes + 1, 8);
    return *reinterpret_cast<int64_t*>(bytes);
}

int32_t Stream::readIndex()
{
    bool isPositive;
    int32_t lastIndex;
    int32_t index;
    uint8_t b0 = readUInt8();
    if (b0 == 0xff) {
        b0 = readUInt8();
        isPositive = false;
        lastIndex = d_lastNegativeIndexRead;
    } else if (b0 == 0) {
        return INDEX_DONE;
    } else {
        isPositive = true;
        lastIndex = d_lastPositiveIndexRead;
    }

    if (b0 == 0xfe) {
        b0 = readUInt8();
        uint8_t b1 = readUInt8();
        if (b0 & 0x80) {
            int32_t b2 = readUInt8();
            int32_t b3 = readUInt8();
            index = b1 + (b2 << 8) + (b3 << 16) + (int32_t(b0 & 0x7f) << 24);
        } else {
            index = (int32_t(b0) << 8) + b1 + lastIndex;
        }
    } else {
        index = b0 + lastIndex;
    }

    if (isPositive) {
        d_lastPositiveIndexRead = index;
    } else {
        d_lastNegativeIndexRead = index;
        index = -index;
    }
    return index;
}

void Stream::writeUInt8(uint8_t i)
{
    write(reinterpret_cast<char *>(&i), 1);
}

void Stream::writeUInt16(uint16_t i)
{
    write(reinterpret_cast<char *>(&i), 2);
}

void Stream::writeInt32(int32_t i)
{
    write(reinterpret_cast<char *>(&i), 4);
}

void Stream::writeInt64(int64_t i)
{
    if (i <= 0x7fffffff && i >= 0) {
        writeInt32(static_cast<int32_t>(i));
        return;
    }

    writeInt32(-1);
    writeInt32(i & 0xffffffff);
    writeInt32(i >> 32);
}

void Stream::writeVariableInt32(int32_t i)
{
    int count = 3;
    uint8_t bytes[5] = { 0 };
    ::memcpy(bytes + 1, &i, sizeof(i));
    while (count > 0 && bytes[count + 1] == 0) {
        --count;
    }
    uint8_t mask = 1 << (7 - count);
    if (bytes[count + 1] >= mask) {
        ++count;
        bytes[0] = ~(mask - 1);
    } else if (count > 0) {
        bytes[0] = bytes[count + 1] | (~(mask * 2 - 1));
    } else {
        bytes[0] = bytes[count + 1];
    }
    write(reinterpret_cast<char *>(bytes), count + 1);
}

void Stream::writeVariableInt64(int64_t i, int minimumBytes)
{
    int count = 7;
    uint8_t bytes[9] = { 0 };
    ::memcpy(bytes + 1, &i, sizeof(i));
    while (count >= minimumBytes && bytes[count + 1] == 0) {
        --count;
    }
    uint8_t mask = 1 << (6 - count + minimumBytes);
    if (bytes[count + 1] >= mask) {
        ++count;
        bytes[0] = ~(mask - 1);
    } else if (count >= minimumBytes) {
        bytes[0] = bytes[count + 1] | (~(mask * 2 - 1));
    } else {
        bytes[0] = bytes[count + 1];
    }
    write(reinterpret_cast<char *>(bytes), count + 1);
}

void Stream::writeIndex(int32_t index)
{
    int32_t diff;
    char bytes[6];
    char *ptr = bytes;
    if (index >= 0) {
        diff = index - d_lastPositiveIndexWritten;
        d_lastPositiveIndexWritten = index;
    } else if (index == INDEX_DONE) {
        writeUInt8(0);
        return;
    } else {
        *ptr++ = 0xffu;
        index = -index;
        diff = index - d_lastNegativeIndexWritten;
        d_lastNegativeIndexWritten = index;
    }

    if (diff > 0 && diff < 0xfe) {
        *ptr++ = diff;
    } else if (diff < 0 || diff > 0x7fff) {
        *ptr++ = 0xfeu;
        *ptr++ = (index >> 24) | 0x80;
        *ptr++ = index & 0xff;
        *ptr++ = index >> 8;
        *ptr++ = index >> 16;
    } else {
        *ptr++ = 0xfeu;
        *ptr++ = diff >> 8;
        *ptr++ = diff & 0xff;
    }
    write(bytes, ptr - bytes);
}

std::string Stream::readLine()
{
    std::string line;
    while (true) {
        char c;
        
        while (readAll(&c, 1) != 1) {
        }
        
        if (c == 0 || c == '\n') {
            return line;
        }
        if (c != '\r') {
            line.push_back(c);
        }
    }
}

void Stream::writeLine(std::string line, bool appendNewLine)
{
    if (appendNewLine && (!line.size() || line[line.size() - 1] != '\n')) {
        line.push_back('\n');
    }
    int size = line.size();
    if (!appendNewLine) {
        ++size;
    }
    writeAll(line.c_str(), size);
}

} // namespace rsync
